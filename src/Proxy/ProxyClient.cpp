/*
 * ProxyClient.cpp
 *
 *  Created on: 2017年6月14日
 *      Author: xzl
 */

#include "ProxyClient.h"
#include "Config.h"
#include "Network/sockutil.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"

using namespace toolkit;

namespace Proxy {

void ProxyCientBase::transfer(const string &to, const string &body, const onResponse &cb) {
    Value obj(objectValue);
    obj["to"] = to;
    sendRequest("transfer", obj, body, cb);
}

void ProxyCientBase::setOnTransfer(const onTransfer &onTransfer) {
    _onTransfer = onTransfer;
}

bool ProxyCientBase::onProcessRequest(const string &cmd, uint64_t seq, const Value &obj, const string &body) {
    typedef void (ProxyCientBase::*onHandleRequest)(const string &cmd, uint64_t seq, const Value &obj, const string &body);
    static unordered_map<string, onHandleRequest> map_cmd_fun;
    static onceToken token([&]() {
        map_cmd_fun.emplace("transfer", &ProxyCientBase::handleRequest_transfer);
    }, nullptr);

    auto it = map_cmd_fun.find(cmd);
    if (it == map_cmd_fun.end()) {
        //这里为了兼容新版本的程序，不应该断开连接
        WarnL << "ProxyCientBase::onProcessRequest 不支持的命令类型:" << cmd << endl;
        sendResponse(seq, CODE_UNSUPPORT, "unsupported cmd", objectValue, "");
        return false;
    }
    auto fun = it->second;
    (this->*fun)(cmd, seq, obj, body);
    return true;
}

void ProxyCientBase::handleRequest_transfer(const string &cmd, uint64_t seq, const Value &obj, const string &body) {
    auto from = obj["from"].asString();
    weak_ptr<ProxyCientBase> weakSelf = getReference();
    if (!_onTransfer) {
        sendResponse(seq, CODE_SUCCESS, "", objectValue, "");
        return;
    }

    onResponse lam = [seq, weakSelf](int code, const string &msg, const Value &obj, const string &body) {
        auto strongSelf = weakSelf.lock();
        if (strongSelf) {
            strongSelf->sendResponse(seq, code, msg, obj, body);
        }
    };
    _onTransfer(from, body, lam);
}

//////////////////////////////ProxyClient////////////////////////////////////

ProxyClient::ProxyClient() : TcpClient(EventPoller::Instance().shared_from_this()) {}

ProxyClient::~ProxyClient() {
    DebugL << "uid:" << _uuid;
}

int ProxyClient::status() const {
    return (int) _status;
}

void ProxyClient::setOnLogin(const onLoginResult &onLogin) {
    _onLogin = onLogin;
}

void ProxyClient::setOnShutdown(const onShutdown &onShutdown) {
    _onShutdown = onShutdown;
}

std::shared_ptr<ProxyCientBase> ProxyClient::getReference() {
    return dynamic_pointer_cast<ProxyCientBase>(shared_from_this());
}

void ProxyClient::login(const string &url, uint16_t port, const string &uuid, const Value &userInfo, bool enableSsl) {
    if (_status != STATUS_LOGOUTED) {
        if (_onLogin) {
            _onLogin(CODE_BAD_LOGIN, "logined or logining");
        }
        return;
    }
    _uuid = uuid;
    _userInfo = userInfo;
    _status = STATUS_LOGINING;
    _enabelSsl = enableSsl;
    _server_url = url;
    static float sec = mINI::Instance()[Config::Proxy::kResTimeoutSec];
    startConnect(url, port, sec);
}

void ProxyClient::logout() {
    if (_status == STATUS_LOGOUTED) {
        //尚未登录
        return;
    }
    disconnect();
}

void ProxyClient::disconnect(const SockException &ex) {
    HttpRequestSplitter::reset();
    shutdown();
    _status = STATUS_LOGOUTED;
    manageResponse(ex.getErrCode(), "ProxyClient::disconnect", true);
    if (ex && _onShutdown) {
        _onShutdown(ex);
    }
}

#define CHECK(status, cb) \
if(status != STATUS_LOGINED){\
    if(cb){\
        cb(CODE_AUTHFAILED,"尚未登陆",objectValue,"");\
    }\
    return;\
}

void ProxyClient::transfer(const string &to, const string &body, const onResponse &cb) {
    CHECK(_status, cb);
    ProxyCientBase::transfer(to, body, cb);
}

void ProxyClient::message(const string &to, const Value &obj, const string &body, const onResponse &cb) {
    CHECK(_status, cb);
    Value obj_tmp = obj;
    obj_tmp["to"] = to;
    sendRequest("message", obj_tmp, body, cb);
}

void ProxyClient::joinRoom(const string &room_id, const onResponse &cb) {
    CHECK(_status, cb);
    Value obj(objectValue);
    obj["room_id"] = room_id;
    sendRequest("joinRoom", obj, "", cb);
}

void ProxyClient::exitRoom(const string &room_id, const onResponse &cb) {
    CHECK(_status, cb);
    Value obj(objectValue);
    obj["room_id"] = room_id;
    sendRequest("exitRoom", obj, "", cb);
}

void ProxyClient::broadcastRoom(const string &room_id, const string &body, const onResponse &cb) {
    CHECK(_status, cb);
    Value obj(objectValue);
    obj["room_id"] = room_id;
    sendRequest("broadcast", obj, body, cb);
}

void ProxyClient::onConnect(const SockException &ex) {
    if (ex) {
        _status = STATUS_LOGOUTED;
        if (_onLogin) {
            _onLogin(ex.getErrCode(), "connect server failed");
        }
        return;
    }
    static int timeoutSec = mINI::Instance()[Config::Session::kTimeoutSec];
    getSock()->setSendTimeOutSecond(timeoutSec);

    makeSslBox();
    sendRequest_login();
}

class ConflictException : public std::exception {
public:
    ConflictException(const string &ip, const Value &obj) : _ip(ip), _obj(obj) {}

    ~ConflictException() override {}

    const char *what() const noexcept override {
        return _ip.data();
    }

    const string &getIp() const {
        return _ip;
    }

    const Value &getObj() const {
        return _obj;
    }

private:
    string _ip;
    Value _obj;
};

void ProxyClient::onDecData(const char *data, uint32_t len) {
    try {
        onRecvData(data, len);
    } catch (ConflictException &ex) {
        WarnL << "挤占掉线:" << ex.getIp();
        disconnect(SockException((ErrCode) CODE_CONFLICT, ex.getIp().data()));
    } catch (std::exception &ex) {
        WarnL << ex.what();
        disconnect(SockException((ErrCode) CODE_OTHER, ex.what()));
    }
}

void ProxyClient::onEncData(const char *data, uint32_t len) {
    Status::Instance().addCountSignal(false, len);
    SockSender::send(data, len);
}

void ProxyClient::makeSslBox() {
    _sslBox.reset(new SSL_Box(false, _enabelSsl));
    _sslBox->setOnDecData([this](const Buffer::Ptr &pBuf) {
        onDecData(pBuf->data(), pBuf->size());
    });
    _sslBox->setOnEncData([this](const Buffer::Ptr &pBuf) {
        onEncData(pBuf->data(), pBuf->size());
    });
    if(_enabelSsl){
        _sslBox->setHost(_server_url.data());
    }
}

void ProxyClient::onErr(const SockException &ex) {
    TimeTicker();
    DebugL << ex.what();
    auto status = _status;
    _status = STATUS_LOGOUTED;
    manageResponse(ex.getErrCode(), ex.what(), true);

    if (status == STATUS_LOGINED && ex.getErrCode() != Err_shutdown) {
         if (_onShutdown) {
            _onShutdown(ex);
        }
    }
}

void ProxyClient::onRecv(const Buffer::Ptr &pBuf) {
    Status::Instance().addCountSignal(true, pBuf->size());
    if (_sslBox) {
        _sslBox->onRecv(pBuf);
    }
}

int ProxyClient::onSendData(const string &data) {
    if (_sslBox) {
        _sslBox->onSend(std::make_shared<BufferString>(data));
    }
    return data.size();
}

bool ProxyClient::onProcessRequest(const string &cmd, uint64_t seq, const Value &obj, const string &body) {
    typedef void (ProxyClient::*onHandleRequest)(const string &cmd, uint64_t seq, const Value &obj, const string &body);
    static unordered_map<string, onHandleRequest> map_cmd_fun;
    static onceToken token([&]() {
        map_cmd_fun.emplace("conflict", &ProxyClient::handleRequest_conflict);
        map_cmd_fun.emplace("message", &ProxyClient::handleRequest_message);
        map_cmd_fun.emplace("onJoin", &ProxyClient::handleRequest_onJoinRoom);
        map_cmd_fun.emplace("onExit", &ProxyClient::handleRequest_onExitRoom);
        map_cmd_fun.emplace("broadcast", &ProxyClient::handleRequest_onBroadcast);
    });

    auto it = map_cmd_fun.find(cmd);
    if (it == map_cmd_fun.end()) {
        return ProxyCientBase::onProcessRequest(cmd, seq, obj, body);
    }
    auto fun = it->second;
    (this->*fun)(cmd, seq, obj, body);
    return true;
}

void ProxyClient::sendBeat() {
    sendRequest("beat", Json::objectValue, "",[](int code, const string &msg, const Value &obj, const string &body) {
        if (code != CODE_SUCCESS) {
            WarnL << "beat response:" << code << " " << msg;
        }
    });
}

void ProxyClient::sendRequest_login() {
    Value obj(objectValue);
    obj["uuid"] = _uuid;
    obj["info"] = _userInfo;
    sendRequest("login", obj, "", [this](int code, const string &msg, const Value &obj, const string &body) {
        InfoL << "login result:" << code << " " << msg;
        if (code == CODE_JUMP) {
            //跳转服务器
            auto jumpSrvUrl = obj["jumpSrvUrl"].asString();
            uint16_t jumpSrvPort = obj["jumpSrvPort"].asInt();
            InfoL << "jump to:" << jumpSrvUrl << ":" << jumpSrvPort;
            startConnect(jumpSrvUrl, jumpSrvPort);
            return;
        }

        _status = (code == CODE_SUCCESS ? STATUS_LOGINED : STATUS_LOGOUTED);
        if (_onLogin) {
            _onLogin(code, msg);
        }
    });
}

void ProxyClient::handleRequest_conflict(const string &cmd, uint64_t seq, const Value &obj, const string &body) {
    auto ip = obj["ip"].asString();
    sendResponse(seq, CODE_SUCCESS, "", objectValue, body);
    throw ConflictException(ip, obj);
}

void ProxyClient::handleRequest_message(const string &cmd, uint64_t seq, const Value &obj, const string &body) {
    auto from = obj["from"].asString();
    weak_ptr<ProxyCientBase> weakSelf = getReference();
    InfoL << cmd << " from:" << from << " " << body;
    onResponse invoker = [weakSelf, seq](int code, const string &msg, const Value &obj, const string &body) {
        EventPoller::Instance().async([weakSelf, seq, code, msg, obj, body]() {
            auto strongSelf = weakSelf.lock();
            if (strongSelf) {
                strongSelf->sendResponse(seq, code, msg, obj, body);
            }
        });
    };
    NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastOnMessage, from, obj, body, invoker);
}

void ProxyClient::handleRequest_onRoomEvent(const string &cmd, uint64_t seq, const Value &obj, const string &body, const char *event) {
    auto from = obj["from"].asString();
    auto room_id = obj["room_id"].asString();
    weak_ptr<ProxyCientBase> weakSelf = getReference();
    onResponse invoker = [weakSelf, seq](int code, const string &msg, const Value &obj, const string &body) {
        EventPoller::Instance().async([weakSelf, seq, code, msg, obj, body]() {
            auto strongSelf = weakSelf.lock();
            if (strongSelf) {
                strongSelf->sendResponse(seq, code, msg, obj, body);
            }
        });
    };
    NoticeCenter::Instance().emitEvent(event, from, room_id, obj, body, invoker);
}

void ProxyClient::handleRequest_onJoinRoom(const string &cmd, uint64_t seq, const Value &obj, const string &body) {
    handleRequest_onRoomEvent(cmd, seq, obj, body, Config::Broadcast::kBroadcastOnJoinRoom);
}

void ProxyClient::handleRequest_onExitRoom(const string &cmd, uint64_t seq, const Value &obj, const string &body) {
    handleRequest_onRoomEvent(cmd, seq, obj, body, Config::Broadcast::kBroadcastOnExitRoom);
}

void ProxyClient::handleRequest_onBroadcast(const string &cmd, uint64_t seq, const Value &obj, const string &body) {
    handleRequest_onRoomEvent(cmd, seq, obj, body, Config::Broadcast::kBroadcastOnRoomBroadcast);
}

void ProxyClient::onManager() {
    TimeTicker();
    static int timeoutSec = mINI::Instance()[Config::Session::kTimeoutSec];
    static int beatInterval = mINI::Instance()[Config::Session::kBeatInterval];
    if (_status == STATUS_LOGINED && _aliveTicker.elapsedTime() >= timeoutSec * 1000) {
        //规定时间内未检测到设备的正常活动（设备可能拔掉网线了或NAT映射结束），断开连接
        WarnL << "与服务器间的连接已超时！";
        disconnect(SockException((ErrCode)CODE_TIMEOUT, "connection timeouted"));
        return;
    }

    if (_status == STATUS_LOGINED && _aliveTicker.elapsedTime() > beatInterval * 1000) {
        sendBeat();
    }

    manageResponse(CODE_TIMEOUT, "ProxyClient timeout");
}

} /* namespace Proxy */
