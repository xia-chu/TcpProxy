/*
 * TcpProxySession.cpp
 *
 *  Created on: 2017年6月12日
 *      Author: xzl
 */

#include "ProxySession.h"
#include "Config.h"
#include "Util/onceToken.h"
#include "Network/TcpServer.h"
#include "Util/NoticeCenter.h"
using namespace toolkit;

namespace Proxy {

static atomic<uint64_t> s_sessionCount(0);

uint64_t ProxySession::getSessionCount(){
    return s_sessionCount;
}

ProxySession::ProxySession(const Socket::Ptr &sock)
    : Session(sock)
    , ProxyProtocol(TYPE_SERVER) {
    DebugP(this);
    getSock()->setSendTimeOutSecond(mINI::Instance()[Config::Session::kTimeoutSec]);
    ++s_sessionCount;
}

ProxySession::~ProxySession() {
    DebugP(this) << "uid:" << _user_id;
    --s_sessionCount;
}

const Value& ProxySession::getLoginInfo() const{
    return _login_info;
}

const string &ProxySession::getSessionUuid() const{
    return _user_id;
}

const string &ProxySession::getSessionAppId() const {
    return _app_id;
}

void ProxySession::onRecv(const Buffer::Ptr &pBuf) {
    try {
        onRecvData(pBuf->data(), pBuf->size());
    } catch (std::exception &ex) {
        shutdown(SockException(Err_shutdown, string("catch exception:") + ex.what()));
    }
}

void ProxySession::onError(const SockException& err) {
    TimeTicker();
    WarnP(this) << err.what() << ":" << _user_id;
    manageResponse(err.getErrCode(), err.what(), true);
    onLogout();
}

void ProxySession::onManager() {
    TimeTicker();
    static int timeoutSec = mINI::Instance()[Config::Session::kTimeoutSec];

    if (_status != STATUS_LOGINED && _aliveTicker.createdTime() > 10 * 1000) {
        //设备10秒内未登录，判断为非法连接
        WarnP(this) << "规定时间未登录,ip:" << get_peer_ip() << ",静止时间:" << _aliveTicker.createdTime() << "ms";
        shutdown(SockException(Err_shutdown, "规定时间内未登录"));
        return;
    }

    int elapsedTime = _aliveTicker.elapsedTime();
    if (elapsedTime >= timeoutSec * 1000) {
        //规定时间内未检测到设备的正常活动（设备可能拔掉网线了或NAT映射结束），断开连接
        WarnP(this) << "不活跃的会话,ip:" << get_peer_ip() << ",uid:" << _user_id << ",静止时间:" << elapsedTime << "ms";
        shutdown(SockException(Err_shutdown, "心跳超时"));
        return;
    }

    manageResponse(CODE_TIMEOUT, "ProxySession timeout");
}

bool ProxySession::onProcessRequest(const string &cmd,uint64_t seq, const Value &obj,const string &body) {
    typedef void (ProxySession::*onHandleRequest)(const string &cmd, uint64_t seq, const Value &obj, const string &body);
    static unordered_map<string, onHandleRequest> map_cmd_fun;
    static onceToken token([&]() {
        map_cmd_fun.emplace("login", &ProxySession::handleRequest_login);
        map_cmd_fun.emplace("transfer", &ProxySession::handleRequest_transfer);
        map_cmd_fun.emplace("message", &ProxySession::handleRequest_transfer);
        map_cmd_fun.emplace("joinRoom", &ProxySession::handleRequest_joinRoom);
        map_cmd_fun.emplace("exitRoom", &ProxySession::handleRequest_exitRoom);
        map_cmd_fun.emplace("broadcast", &ProxySession::handleRequest_broadcastRoom);
        map_cmd_fun.emplace("beat", &ProxySession::handleRequest_beat);
    });

    auto it = map_cmd_fun.find(cmd);
    if (it == map_cmd_fun.end()) {
        //这里为了兼容新版本的程序，不应该断开连接
        WarnP(this) << "ProxySession::onProcessRequest 不支持的命令类型:" << cmd << endl;
        sendResponse(seq, CODE_UNSUPPORT, "unsupported cmd", objectValue);
        return false;
    }
    auto fun = it->second;
    (this->*fun)(cmd, seq, obj, body);
    return true;
}

static void pushTask(const weak_ptr<Session> &weakPtr, const function<void(bool)> &func) {
    auto strongSelf = weakPtr.lock();
    if (!strongSelf) {
        func(false);
        return;
    }
    strongSelf->async([weakPtr, func]() {
        auto strongSelf = weakPtr.lock();
        if (!strongSelf) {
            func(false);
            return;
        }
        func(true);
    });
}

void ProxySession::handleRequest_login(const string &cmd, uint64_t seq, const Value &obj,const string &body) {
    InfoP(this) << obj;
    TimeTicker();
    /*
     * obj 对象的定义：
     * {
     * 	"uuid":"该会话的唯一识别符",
     *  "ver" : "协议版本，1.0"
     * }
     */
    checkState(cmd,STATUS_LOGOUTED);

    //登录中
    _status = STATUS_LOGINING;
    auto appId = obj["info"]["user"]["appId"].asString();
    auto token = obj["info"]["user"]["token"].asString();
    auto uuid = obj["uuid"].asString();
    if(uuid.empty() || appId.empty() || token.empty()){
        sendResponse(seq,CODE_BAD_REQUEST,"uuid 、appId 、token 字段为空",Json::objectValue);
        throw std::runtime_error("uuid 、appId 、token 字段为空!");
    }

    weak_ptr<ProxySession> weakSelf = dynamic_pointer_cast<ProxySession>(shared_from_this());
    AuthInvoker invoker = [weakSelf, this, seq, obj, appId, uuid](const string &err) {
        pushTask(weakSelf, [this, seq, obj, appId, uuid, err](bool flag) {
            if (flag) {
                if (!err.empty()) {
                    //鉴权失败
                    sendResponse(seq, CODE_AUTHFAILED, err, Json::objectValue);
                    shutdown(SockException(Err_shutdown, err));
                    return;
                }
                //鉴权成功
                onAuthSuccess(seq, appId, uuid, obj);
            }
        });
    };

    //鉴权事件广播
    auto strongSelf = shared_from_this();
    if(!NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastOnUserLoginAuth, strongSelf, appId, uuid, token, obj, invoker)){
        invoker("");
    }
}

static void saveMessageHistory( const string &appId, const string &from, const string &to, const Value &obj, const string &body, bool success, bool roomMessage){
    NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastOnMessageHistory, appId, from, to, obj, body, success, roomMessage);
}

void ProxySession::handleRequest_transfer(const string &cmd,uint64_t seq, const Value& obj,const string &body) {
    TimeTicker();
    /*
     * obj 对象的定义：
     * {
     * 	"to":"转发目标",
     * }
     */

    checkState(cmd, STATUS_LOGINED);
    auto to_uid = obj["to"].asString();
    if (to_uid == _user_id) {
        sendResponse(seq, CODE_NOTFOUND, "can not send message to yourself", objectValue);
        return;
    }

    auto &appId = _app_id;
    auto &from = _user_id;
    auto saveMessage = [appId, from, to_uid, cmd, obj, body](bool success) {
        if (cmd != "message") {
            //transfer命令忽略之，我们只保存message命令的消息记录
            return;
        }
        saveMessageHistory(appId, from, to_uid, obj, body, success, false);
    };

    auto targetSession = UserManager::Instance(_app_id).get(to_uid);
    if(!targetSession){
        weak_ptr<ProxySession> weakSelf = dynamic_pointer_cast<ProxySession>(shared_from_this());
        ResponseInvoker invoker = [seq, weakSelf, this, saveMessage](int code, const string &msg, const Value &obj, const string &body) {
            pushTask(weakSelf, [this, seq, code, msg, obj, body, saveMessage](bool flag) {
                if (flag) {
                    //回复转发结果
                    sendResponse(seq, code, msg, obj, body);
                }
                saveMessage(code == CODE_SUCCESS);
            });
        };

        auto flag = NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastOnUserMessage, shared_from_this(), _app_id, _user_id, to_uid, cmd, obj, body, invoker);
        if (!flag) {
            //无人监听事件
            saveMessage(false);
            sendResponse(seq, CODE_NOTFOUND, "can not find the target", objectValue);
        }
        return;
    }

    weak_ptr<ProxySession> weakSelf = dynamic_pointer_cast<ProxySession>(shared_from_this());
    //驱动目标会话执行转发
    targetSession->sendRequestFromOther(cmd, _user_id, obj, body, [weakSelf, seq, this , saveMessage](int code, const string &msg, const Value &obj, const string &body) {
        pushTask(weakSelf, [this, seq, code, msg, obj, body, saveMessage](bool flag) {
            if (flag) {
                //回复转发结果
                sendResponse(seq, code, msg, obj, body);
            }
            saveMessage(code == CODE_SUCCESS);
        });
    });
}

void ProxySession::handleRequest_joinRoom(const string &cmd, uint64_t seq, const Value &obj, const string &body) {
    checkState(cmd,STATUS_LOGINED);

    auto room_id = obj["room_id"].asString();
    if (room_id.empty()) {
        //房间名无效
        throw std::runtime_error("房间名无效");
    }
    if (_joined_rooms.find(room_id) != _joined_rooms.end()) {
        //重复加入
        sendResponse(seq, CODE_SUCCESS, "success", objectValue);
        return;
    }

    weak_ptr<ProxySession> weakSelf = dynamic_pointer_cast<ProxySession>(shared_from_this());
    RoomManager::Instance(_app_id).joinRoom(room_id, _user_id, weakSelf.lock(), [weakSelf, this, room_id, seq, obj, body](const string &err) {
        //加入房间结果
        pushTask(weakSelf, [this, room_id, seq, obj, body, err](bool flag) {
            if (flag) {
                if (!err.empty()) {
                    //加入房间失败
                    sendResponse(seq, CODE_AUTHFAILED, err, objectValue);
                    return;
                }

                //加入房间成功
                sendResponse(seq, CODE_SUCCESS, "success", objectValue);

                if (!_joined_rooms.emplace(room_id).second) {
                    //重复加入
                    return;
                }
                broadcastRoomEvent("onJoin", room_id, obj, body, false);
            }
        });
    });
}

void ProxySession::handleRequest_exitRoom(const string &cmd, uint64_t seq, const Value &obj, const string &body) {
    checkState(cmd, STATUS_LOGINED);
    exitRoom(obj["room_id"].asString(), true);
    sendResponse(seq, CODE_SUCCESS, "success", objectValue);
}

void ProxySession::exitRoom(const string &room_id, bool emitEvent) {
    //退出单个群组
    if (!room_id.empty()) {
        //通知其他人我已经退出房间了
        if (_joined_rooms.erase(room_id)) {
            broadcastRoomEvent("onExit", room_id, objectValue, "", false);
        }

        //从房间列表中移除自己
        if (emitEvent) {
            RoomManager::Instance(_app_id).exitRoom({room_id}, _user_id, dynamic_pointer_cast<ProxySession>(shared_from_this()), emitEvent);
        }
        return;
    }

    //退出全部群组
    decltype(_joined_rooms) joinedRooms;
    joinedRooms = _joined_rooms;
    for (auto &room : joinedRooms) {
        if (!room.empty()) {
            exitRoom(room, false);
        }
    }
    _joined_rooms.clear();
    RoomManager::Instance(_app_id).exitRoom(joinedRooms, _user_id, dynamic_pointer_cast<ProxySession>(shared_from_this()), emitEvent);
}

bool ProxySession::broadcastRoomEvent(const string &cmd, const string &room_id, const Value &obj_in, const string &body,bool emitEvent) {
    Value obj(obj_in);
    obj["room_id"] = room_id;
    //广播消息
    bool flag = RoomManager::Instance(_app_id).forEachRoomMember(room_id, _user_id, [&](Ptr &ptr) {
        if (ptr.get() == this) {
            //忽略自己
            return;
        }
        ptr->sendRequestFromOther(cmd, _user_id, obj, body, [](int code, const string &msg, const Value &obj, const string &body) {});
    });

    if (!flag || !emitEvent) {
        //未广播消息成功
        return false;
    }

    //我在房间内,把房间广播信息发送给事件服务器
    ResponseInvoker invoker = [](int code, const string &msg, const Value &obj, const string &body) {};
    NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastOnUserMessage, shared_from_this(), _app_id, _user_id, room_id, cmd, obj, body, invoker);
    return true;
}

void ProxySession::handleRequest_broadcastRoom(const string &cmd, uint64_t seq, const Value &obj, const string &body) {
    auto room_id = obj["room_id"].asString();
    if (room_id.empty()) {
        throw std::runtime_error("房间名无效");
    }

    auto flag = broadcastRoomEvent("broadcast", room_id, obj, body, true);
    sendResponse(seq, flag ? CODE_SUCCESS : CODE_OTHER, flag ? "success" : "you are not in this room", objectValue);
    saveMessageHistory(_app_id, _user_id, room_id, obj, body, flag, true);
}

void ProxySession::handleRequest_beat(const string &cmd,uint64_t seq, const Value &obj,const string &body){
    sendResponse(seq,CODE_SUCCESS,"",objectValue);
}

void ProxySession::onAuthSuccess(int seq, const string &appId, const string &uuid, const Value &obj) {
    TimeTicker();
    _status = STATUS_LOGINED;
    _user_id = uuid;
    _app_id = appId;
    _login_info = obj;
    onLogin();
    sendResponse(seq, CODE_SUCCESS, "auth success", Json::objectValue);
}

void ProxySession::onLogin() {
    TimeTicker();
    auto strongSession = UserManager::Instance(_app_id).get(_user_id);
    if (strongSession && strongSession.get() != this) {
        //其他会话已经占用该uuid，通知它下线
        strongSession->onConflict(get_peer_ip());
    }

    //记录本对象；emplace操作不能覆盖
    UserManager::Instance(_app_id).add(_user_id, dynamic_pointer_cast<ProxySession>(shared_from_this()));
    NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastOnUserLog, shared_from_this(), true, _app_id, _user_id);
}

void ProxySession::onLogout() {
    TimeTicker();
    if (UserManager::Instance(_app_id).del(_user_id, this)) {
        NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastOnUserLog, shared_from_this(), false, _app_id, _user_id);
    }
    exitRoom("", false);
}

void ProxySession::onConflict(const string &ip) {
    pushTask(shared_from_this(), [this, ip](bool flag) {
        if (flag) {
            TimeTicker();
            Value obj(objectValue);
            obj["ip"] = ip;
            sendRequest("conflict", obj, "", [this, ip](int code, const string &msg, const Value &obj, const string &body) {
                //断开连接
                if (CODE_OTHER != code) {
                    //防止触发登出事件
                    UserManager::Instance(_app_id).del(_user_id, this);
                    shutdown(SockException(Err_shutdown, string("conflict:") + ip));
                }
            });
        }
    });
}

void ProxySession::sendRequestFromOther(const string &cmd, const string &from, const Value &obj_in, const string &body, const onResponse &cb) {
    pushTask(shared_from_this(), [this, cmd, from, obj_in, body, cb](bool flag) {
        if (!flag) {
            cb(CODE_NOTFOUND, "the target is released", objectValue, "");
            return;
        }
        TimeTicker();
        Value obj(obj_in);
        obj["from"] = from;
        sendRequest(cmd, obj, body, cb);
    });
}

int ProxySession::onSendData(const string &data) {
    return SockSender::send(data);
}

void ProxySession::checkState(const string &cmd,STATUS_CODE state) {
    if (_status != state) {
        auto strErr = StrPrinter << "会话状态:" << _status << ",不得发送" << cmd << "命令." << endl;
        throw std::runtime_error(strErr);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static unordered_map<string,UserManager::Ptr> s_userInstance;
static recursive_mutex s_userMtx;
UserManager &UserManager::Instance(const string &appId) {
    lock_guard<recursive_mutex> lck(s_userMtx);
    auto it = s_userInstance.find(appId);
    if (it == s_userInstance.end()) {
        UserManager::Ptr ret(new UserManager(appId));
        s_userInstance.emplace(appId, ret);
        return *ret;
    }
    return *(it->second);
}

UserManager::UserManager(const string &appId){
    _appId = appId;
}

void UserManager::add(const string &user_name, const ProxySession::Ptr &session) {
    lock_guard<recursive_mutex> lck(_mtxSession);
    _mapSession[user_name] = session;
}

bool UserManager::del(const string &user_name, ProxySession *obj) {
    lock_guard<recursive_mutex> lck(_mtxSession);
    auto it = _mapSession.find(user_name);
    if (it == _mapSession.end()) {
        return false;
    }
    auto strongSession = it->second.lock();
    if (!strongSession) {
        _mapSession.erase(it);
        return false;
    }
    if (strongSession.get() != obj) {
        //这是同名的其他人
        return false;
    }
    _mapSession.erase(it);
    return true;
}

ProxySession::Ptr UserManager::get(const string &user_name) {
    lock_guard<recursive_mutex> lck(_mtxSession);
    auto it = _mapSession.find(user_name);
    if (it == _mapSession.end()) {
        return nullptr;
    }
    auto strongSession = it->second.lock();
    if (!strongSession) {
        _mapSession.erase(it);
    }
    return strongSession;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static unordered_map<string,RoomManager::Ptr> s_roomInstance;
static recursive_mutex s_roomMtx;
RoomManager &RoomManager::Instance(const string &appId) {
    lock_guard<recursive_mutex> lck(s_roomMtx);
    auto it = s_roomInstance.find(appId);
    if (it == s_roomInstance.end()) {
        RoomManager::Ptr ret(new RoomManager(appId));
        s_roomInstance.emplace(appId, ret);
        return *ret;
    }
    return *(it->second);
}

RoomManager::RoomManager(const string &appId){
    _appId = appId;
}

void RoomManager::joinRoom(const string &room_id, const string &user_name, const ProxySession::Ptr &ptr,const ProxySession::AuthInvoker &invoker) {
    lock_guard<recursive_mutex> lck(_mtxRoom);
    auto strongSession = _mapRoom[room_id][user_name].lock();
    if(strongSession == ptr){
        //重复加入房间
        invoker("");
        return;
    }
    weak_ptr<ProxySession> weakPtr = ptr;
    ProxySession::AuthInvoker invokerWrapper = [this,weakPtr,room_id,user_name,invoker](const string &err){
        if (err.empty()) {
            lock_guard<recursive_mutex> lck(_mtxRoom);
            _mapRoom[room_id][user_name] = weakPtr;
            if (_mapRoom[room_id].size() == 1) {
                onAddRoom(room_id);
            }
        }
        invoker(err);
    };

    auto flag = NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastOnUserJoinRoom, ptr, _appId, user_name, room_id, invokerWrapper);
    if(!flag){
        invokerWrapper("");
    }
}

bool RoomManager::exitRoom(const unordered_set<string> &room_ids, const string &user_name, const ProxySession::Ptr &ptr,bool emitEvent) {
    bool flag = false;
    for (auto &room_id : room_ids){
        lock_guard<recursive_mutex> lck(_mtxRoom);

        auto it0 = _mapRoom.find(room_id);
        if(it0 == _mapRoom.end()){
            //无此房间
            continue;
        }
        auto it1 = it0->second.find(user_name);
        if(it1 == it0->second.end()){
            //房间无此人
            continue;
        }

        auto strongSession = it1->second.lock();
        if(strongSession && strongSession != ptr){
            //这是其他同名对象
            continue;
        }

        //从房间删除此人
        it0->second.erase(it1);
        if(it0->second.empty()){
            //房间为空，删除房间
            onRemoveRoom(it0->first);
            _mapRoom.erase(it0);
        }
        flag = true;
    }

    if(flag && emitEvent){
        NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastOnUserExitRoom,ptr,_appId,user_name,room_ids);
    }
    return flag;
}

bool RoomManager::forEachRoomMember(const string &room_id, const string &user_name, const function<void(ProxySession::Ptr &ptr)> &callback) {
    lock_guard<recursive_mutex> lck(_mtxRoom);
    auto it0 = _mapRoom.find(room_id);
    if (it0 == _mapRoom.end()) {
        return false;
    }
    if (!user_name.empty() && it0->second.find(user_name) == it0->second.end()) {
        //我不在此房间
        return false;
    }

    for (auto it1 = it0->second.begin(); it1 != it0->second.end();) {
        auto strongSession = it1->second.lock();
        if (!strongSession) {
            it1 = it0->second.erase(it1);
            continue;
        }
        callback(strongSession);
        ++it1;
    }

    if (it0->second.empty()) {
        onRemoveRoom(it0->first);
        _mapRoom.erase(it0);
    }
    return true;
}

void RoomManager::onAddRoom(const string &room_id) {
    NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastOnRoomCountChanged, _appId, room_id, true);
}

void RoomManager::onRemoveRoom(const string &room_id) {
    NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastOnRoomCountChanged, _appId, room_id, false);
}

} /* namespace Proxy */
