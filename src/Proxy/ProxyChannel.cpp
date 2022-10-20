/*
 * ProxyChannel.cpp
 *
 *  Created on: 2017年6月15日
 *      Author: xzl
 */

#include "ProxyChannel.h"
#include "Util/onceToken.h"
#include "Network/sockutil.h"
#include "Util/NoticeCenter.h"
using namespace toolkit;

namespace Proxy {

//////////////////////ProxySocket//////////////////

void ProxySocket::connectRemote(const string& url, uint16_t port, int timeout) {
    weak_ptr<ProxySocket> weakSelf = shared_from_this();
    setSockid(nullptr);
    _channel->master_requestOpenSocket(url,port,[weakSelf](const SockException& ex, const SockID& sockid){
        auto strongSelf = weakSelf.lock();
        if(strongSelf){
            strongSelf->setSockid(sockid);
            strongSelf->onRemoteConnect(ex);
        }
    },timeout);
}

int ProxySocket::sendRemote(const string& data) {
    if(!_sockid){
        return -1;
    }
    _channel->master_requestSendData(*_sockid,data);
    return data.size();
}

void ProxySocket::enableRecvFromRemote(bool flag){
    _channel->enableAck(flag);
}

void ProxySocket::setSockid(const SockID& sockid) {
    if(_sockid){
        _channel->master_detachEvent(*_sockid);
        _sockid.reset();
    }
    if(sockid){
        _sockid = sockid;
        _channel->master_attachEvent(shared_from_this());
    }
}

ProxySocket::ProxySocket(const std::shared_ptr<ProxyChannel> &channel):_channel(channel){
}

ProxySocket::~ProxySocket(){
    setSockid(nullptr);
}

void ProxySocket::setOnRemoteConnect(const Socket::onErrCB &cb) {
    _onRemoteConnect = cb;
}

void ProxySocket::setOnRemoteErr(const Socket::onErrCB &cb) {
    _onRemoteErr = cb;
}

void ProxySocket::setOnRemoteRecv(const ProxySocket::onRemoteRecv &cb) {
    _onRemoteRecv = cb;
}

void ProxySocket::setOnRemoteSendable(const onRemoteSendable &cb) {
    _onRemoteSendable = cb;
}

void ProxySocket::onRemoteConnect(const SockException &ex){
    if(_onRemoteConnect){
        _onRemoteConnect(ex);
    }else{
        DebugL << ex.getErrCode()  << " " << ex.what();
    }
}

void ProxySocket::onRemoteErr(const SockException &ex){
    _sockid.reset();
    if(_onRemoteErr){
        _onRemoteErr(ex);
    }else{
        DebugL << ex.getErrCode()  << " " << ex.what();
    }
}

void ProxySocket::onRemoteRead(const string &data){
    if(_onRemoteRecv){
        _onRemoteRecv(data);
    }else{
        DebugL << data;
    }
}

void ProxySocket::enableSendRemote(bool enabled){
    if(_onRemoteSendable){
        _onRemoteSendable(enabled);
    }else{
        DebugL << enabled;
    }
}

//////////////////////ProxyChannel//////////////////

bool ProxyChannel::onProcessRequest(const string& cmd, uint64_t seq,const Value& obj, const string& body) {
    typedef void (ProxyChannel::*onHandleRequest)(uint64_t seq, const Value &obj,const string &body);
    static unordered_map<string,onHandleRequest> map_cmd_fun;
    static onceToken token([&](){
        map_cmd_fun.emplace("bind",&ProxyChannel::slave_handleRequest_bind);
        map_cmd_fun.emplace("connect",&ProxyChannel::slave_handleRequest_connect);
        map_cmd_fun.emplace("send",&ProxyChannel::slave_handleRequest_send);
        map_cmd_fun.emplace("close",&ProxyChannel::slave_handleRequest_close);
        map_cmd_fun.emplace("onRecv",&ProxyChannel::master_handleRequest_onRecv);
        map_cmd_fun.emplace("onErr",&ProxyChannel::master_handleRequest_onErr);
    },nullptr);

    auto it = map_cmd_fun.find(cmd);
    if(it == map_cmd_fun.end()){
        //这里为了兼容新版本的程序，不应该断开连接
        WarnL << "ProxyChannel::onProcessRequest 不支持的命令类型:" << cmd << endl;
        sendResponse(seq,CODE_UNSUPPORT,"unsupported cmd",objectValue,"");
        return false;
    }
    auto fun = it->second;
    (this->*fun)(seq,obj,body);
    return true;
}

int ProxyChannel::onSendData(const string &data) {
    return realSend(data);
}

void ProxyChannel::slave_handleRequest_connect(uint64_t seq, const Value &obj,const string &body){
    /*
     * obj 对象的定义：
     * {
     * 	"url":"目标主机ip或域名",
     *  "port" : "目标主机端口号
     *  "timeout":超时时间
     * }
     */

    auto url = obj["url"].asString();
    auto port = obj["port"].asInt();
    auto timeout = obj["timeout"].asInt();

    Socket::Ptr sock(new Socket(EventPoller::Instance().shared_from_this()));
    weak_ptr<ProxyChannel> weakSelf = shared_from_this();
    auto sockId = _slaveCurrentSockId++;
    InfoL << "open socket:" << sockId << " " << url << ":" << port;
    sock->connect(url,port,[seq,weakSelf,sockId](const SockException &err){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        /*
         * obj 对象的定义：
         * {
         * 	"sockid":套接字id
         * 	"msg":"错误描述",
         * 	"code":错误代码
         * }
         */
        Value obj(objectValue);
        if(err){
            //failed
            obj["code"] = err.getErrCode();
            obj["msg"] = err.what();
            obj["sockid"] = -1;
            auto strErr = StrPrinter << "connect failed:" << err.getErrCode() << " " << err.what() << endl;
            strongSelf->sendResponse(seq,CODE_SUCCESS,strErr,obj,"");
            strongSelf->_slaveSockMap.erase(sockId);
            WarnL << "open socket " << sockId << " failed:" << err.what();
        }else{
            //success
            obj["code"] = Err_success;//成功
            obj["msg"] = "success";
            obj["sockid"] = sockId;
            strongSelf->sendResponse(seq,CODE_SUCCESS,"",obj,"");
            InfoL << "open socket " << sockId << " success,total socket count:" << strongSelf->_slaveSockMap.size();

        }
    },timeout);
    sock->setOnRead([weakSelf,sockId](const Buffer::Ptr &buf, struct sockaddr *,int){
        Status::Instance().addCountBindClient(true, buf->size());
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        /*
         * obj 对象的定义：
         * {
         * 	"sockid":套接字id
         * }
         */
        //DebugL << "ProxySlave onRead:" << buf->size();
        Value obj(objectValue);
        obj["sockid"] = sockId;
        string body(buf->data(),buf->size());
        strongSelf->sendRequest("onRecv",obj,body,nullptr);
    });
    sock->setOnErr([weakSelf,sockId](const SockException &err){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        /*
         * obj 对象的定义：
         * {
         * 	"sockid":套接字id
         * 	"msg":"错误描述",
         * 	"code":错误代码
         * }
         */
        Value obj(objectValue);
        obj["code"] = err.getErrCode();
        obj["msg"] = err.what();
        obj["sockid"] = sockId;
        strongSelf->sendRequest("onErr",obj,"",nullptr);
        strongSelf->_slaveSockMap.erase(sockId);
    });

    sock->setOnFlush([weakSelf,sockId](){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return false;
        }
        //slave数据发送出去了，那么回复Ack，允许master端产生数据
        strongSelf->enableAck(true);
        return true;
    });
    _slaveSockMap[sockId] = sock;
}

void ProxyChannel::slave_handleRequest_send(uint64_t seq, const Value &obj,const string &body){
    Status::Instance().addCountBindClient(false, body.size());
    /*
     * obj 对象的定义：
     * {
     * 	"sockid":套接字id
     * }
     */
    //为了提高系统的性能，send命令不需要回复
    auto sockid = obj["sockid"].asInt();
    //InfoL << "send to socket " << sockid << ":" << body.size();
    auto it = _slaveSockMap.find(sockid);
    if(it != _slaveSockMap.end()){
        auto sock = it->second;
        sock->send(body);
        if(sock->isSocketBusy()){
            //slave端数据发送不出去，那么停止ack，让master端降低数据产生速度
            enableAck(false);
        }
    }
}

void ProxyChannel::slave_handleRequest_close(uint64_t seq, const Value &obj,const string &body){
    /*
     * obj 对象的定义：
     * {
     * 	"sockid":套接字id
     * }
     */
    //为了提高系统的性能，close命令不需要回复
    auto sockid = obj["sockid"].asInt();
    InfoL << "close socket:" << sockid;
    _slaveSockMap.erase(sockid);
}

void ProxyChannel::slave_handleRequest_bind(uint64_t seq, const Value &obj,const string &body){
    //需要回复
    InfoL << "bind by:" << _dst_uuid;
    sendResponse(seq,CODE_SUCCESS,"",objectValue,"");
    NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastOnBind,(string &)_dst_uuid);
}

void ProxyChannel::master_handleRequest_onRecv(uint64_t seq, const Value &obj,const string &body){
    /*
     * obj 对象的定义：
     * {
     * 	"sockid":套接字id
     * }
     */
    //为了提高系统的性能，onRecv命令不需要回复
    auto sockid = obj["sockid"].asInt();
    //InfoL << "socket " << sockid << " recv:" << body.size();
    master_onSockData(sockid,body);
}

void ProxyChannel::master_handleRequest_onErr(uint64_t seq, const Value &obj,const string &body){
    /*
     * obj 对象的定义：
     * {
     * 	"sockid":套接字id
     * 	"msg":"错误描述",
     * 	"code":错误代码
     * }
     */
    //为了提高系统的性能，onErr命令不需要回复
    auto sockid = obj["sockid"].asInt();
    auto code = obj["code"].asInt();
    auto msg = obj["msg"].asString();
    InfoL << "socket " << sockid << " err:" << code << " " << msg;
    SockException ex((ErrCode)code,msg);
    master_onSockErr(sockid,ex);
}

void ProxyChannel::master_requestSendData(int sockid,const string &data) {
    /*
     * obj 对象的定义：
     * {
     * 	"sockid":套接字id
     * }
     */
    //为了提高系统的性能，close命令不需要回复
    Value obj(objectValue);
    obj["sockid"] = sockid;
    //InfoL << "socket " << sockid << " send:" << data.size();
    sendRequest("send",obj,data,nullptr);
}

void ProxyChannel::master_requestCloseSocket(int sockid) {
    /*
     * obj 对象的定义：
     * {
     * 	"sockid":套接字id
     * }
     */
    //为了提高系统的性能，close命令不需要回复
    Value obj(objectValue);
    obj["sockid"] = sockid;
    //InfoL << "close socket: " << sockid;
    sendRequest("close",obj,"",nullptr);
}

void ProxyChannel::master_requestOpenSocket(const string& url,uint16_t port,const onNewSock& cb, int timeout) {
    /*
     * obj 对象的定义：
     * {
     * 	"url":"目标主机ip或域名",
     *  "port" : "目标主机端口号
     *  "timeout":超时时间
     * }
     */
    Value obj(objectValue);
    obj["url"] = url;
    obj["port"] = port;
    obj["timeout"] = timeout;
    weak_ptr<ProxyChannel> weakSelf = shared_from_this();
    sendRequest("connect",obj,"",[weakSelf,cb](int code,const string &msg,const Value &obj,const string &body){
        /*
         * obj 对象的定义：
         * {
         * 	"sockid":套接字id
         * 	"msg":"错误描述",
         * 	"code":错误代码
         * }
         */
        if(code != CODE_SUCCESS){
            auto errDetail = (StrPrinter << "code:" << code << ",msg:" << msg << endl);
            SockException ex(Err_other,errDetail);
            if(cb){
                cb(ex,nullptr);
            }
        }else{
            auto sockid = obj["sockid"].asInt();
            auto code = obj["code"].asInt();
            auto msg = obj["msg"].asString();
            SockID sockfd;
            SockException ex((ErrCode)code,msg);
            if(!ex){
                //成功
                sockfd.reset(new int(sockid),[weakSelf](int *sockid){
                    auto strongSelf = weakSelf.lock();
                    if(strongSelf){
                        strongSelf->master_requestCloseSocket(*sockid);
                    }
                    delete sockid;
                });
            }
            if(cb){
                cb(ex,sockfd);
            }
        }
    });
}

ProxyChannel::ProxyChannel(const string &dst_uuid):ProxyProtocol(TYPE_CHANNEL){
    static float minNoAckBufferSize = mINI::Instance()[Config::Proxy::kMinNoAckBufferSize];
    static int minPktSize = mINI::Instance()[Config::Proxy::kMinPktSize];
    _dst_uuid = dst_uuid;
    _pktSize =  minPktSize;
    _maxNoAckBytes = minNoAckBufferSize;
    InfoL << _dst_uuid;
}

ProxyChannel::~ProxyChannel(){
    InfoL;
    detachAllSock();
    manageResponse(CODE_OTHER,"~ProxyChannel",true);
}

void ProxyChannel::setOnSend(const onSend& onSend) {
    _onSend = onSend;
}

bool ProxyChannel::expired() {
    static int sec = mINI::Instance()[Config::Proxy::kChnTimeoutSec];
    //如果超过sec秒ProxyChannel都没有收到有效数据则认为ProxyChannel已经过期
    return _aliveTicker.elapsedTime() >= sec * 1000;
}

void ProxyChannel::bind(const onResponse &cb){
    sendRequest("bind",objectValue,"",cb);
}

int ProxyChannel::realSend(const string &data,bool flush){
    if(!_onSend){
        return -1;
    }
    if(!data.empty()){
        _sendBuf.append(data);
    }
    static int optimizeDelaySpeed = mINI::Instance()[Config::Proxy::kOptimizeDelaySpeed];
    //低网速的情况下为了提高响应速度，不缓存
    bool optimizeDelayForLowSpeed = (_bytesAckPerSec < optimizeDelaySpeed * 1024);
    if(!flush && _sendBuf.size() < _pktSize && !optimizeDelayForLowSpeed ){
        return 0;
    }
    //拆包
    int remain = 0;
    int size;
    while(!_sendBuf.empty() && _enableGenerateData){
        //剩余字节数
        remain = _sendBuf.size();
        //本次发送字节数
        size = remain < _pktSize ? remain : _pktSize;
        //本次发送的包
        auto pkt = _sendBuf.substr(0,size);
        _sendBuf.erase(0,size);
        sendPacket(pkt);

        if(_sendBuf.size() < _pktSize && !flush){
            //数据不够，留给下次发送
            break;
        }
    }
    return _sendBuf.size();
}

void ProxyChannel::master_attachEvent(const ProxySocket::Ptr &obj){
    _masterListenerMap.emplace(*(obj->_sockid),obj);
}

void ProxyChannel::master_detachEvent(int sockid){
    _masterListenerMap.erase(sockid);
}

void ProxyChannel::master_onSockData(int sockid,const string &data){
    auto it = _masterListenerMap.find(sockid);
    if(it == _masterListenerMap.end()){
        //没有任何对象监听该事件，关闭Socket
        master_requestCloseSocket(sockid);
        return;
    }
    auto strongListener = it->second.lock();
    if(!strongListener){
        //监听者已销毁，关闭Socket
        _masterListenerMap.erase(it);
        master_requestCloseSocket(sockid);
        return;
    }
    strongListener->onRemoteRead(data);
}

void ProxyChannel::master_onSockErr(int sockid,const SockException &ex){
    auto it = _masterListenerMap.find(sockid);
    if(it == _masterListenerMap.end()){
        //没有任何对象监听该事件
        return;
    }
    auto strongListener = it->second.lock();
    if(!strongListener){
        //监听者已销毁
        _masterListenerMap.erase(it);
        return;
    }
    strongListener->onRemoteErr(ex);
}

void ProxyChannel::onErr(int code,const string &errMsg){
    //整个通道出现异常
    auto errDetail = (StrPrinter << "code:" << code << ",msg:" << errMsg << endl);
    SockException err(Err_other,errDetail);
    for(auto &pr : _masterListenerMap){
        auto strongListener = pr.second.lock();
        if(strongListener){
            strongListener->onRemoteErr(err);
        }
    }
    _masterListenerMap.clear();
    _slaveSockMap.clear();
    manageResponse(code,errMsg,true);
}

void ProxyChannel::onSendSuccess(int count){
    //发送数据成功，更新_aliveTicker计时
    _aliveTicker.resetTime();
    _noAckBytes -= count;
    _totalAckBytes += count;
    //缓存小于_maxBufSize / 2 就开始重新接收数据
    if(_noAckBytes <= _maxNoAckBytes / 2 && enableGenerateData(true)){
        InfoL << "解除上传流控,包大小为" << _pktSize / 1024 << "KB"
              << ",数据产生速率为:" << _bytesAckPerSec / 1024 << "KB/s"
              << ",最大允许未确认缓存为:" << _maxNoAckBytes / 1024 << "KB"
              << ",未确认缓存为:" << _noAckBytes / 1024 << "KB";
    }
}

void ProxyChannel::addSendCount(int size){
    _noAckBytes += size;
    //缓存大于maxBufSize就停止接收数据
    if (_noAckBytes >= _maxNoAckBytes && enableGenerateData(false)) {
        computeDataGenerateSeed();
        InfoL << "开始上传流控,包大小为" << _pktSize / 1024 << "KB"
              << ",数据产生速率为:" << _bytesAckPerSec / 1024 << "KB/s"
              << ",最大允许未确认缓存为:" << _maxNoAckBytes / 1024 << "KB"
              << ",未确认缓存为:" << _noAckBytes / 1024 << "KB";
    }
}

void ProxyChannel::sendPacket(const string &data){
    //从上次写过的时间开始计时
    _flushTicker.resetTime();
    _onSend(data) ;
}

bool ProxyChannel::isEnableGenerateData() const{
    return _enableGenerateData;
}

//设置是否允许让转发数据
bool ProxyChannel::enableGenerateData(bool enabled){
    if (_enableGenerateData == enabled) {
        return false;
    }
    _enableGenerateData = enabled;
    for (auto &pr : _slaveSockMap) {
        auto sock = pr.second;
        if (sock) {
            //未ack的数据太多了，我们应该从源头控制数据产生速度
            //控制slave端，不要再接收数据了
            sock->enableRecv(enabled);
        }
    }
    for (auto &pr : _masterListenerMap) {
        auto strongSock = pr.second.lock();
        if (strongSock) {
            //未ack的数据太多了，我们应该从源头控制数据产生速度
            //控制master端，不要再接收数据了
            strongSock->enableSendRemote(enabled);
        }
    }
    return true;
}

void ProxyChannel::detachAllSock(){
    //通知所有ProxySocket对象已经断开联系
    decltype(_masterListenerMap) tmp;
    tmp.swap(_masterListenerMap);

    SockException ex(Err_other,"ProxyChannel released");
    for(auto &pr : tmp){
        auto strongSock = pr.second.lock();
        if(strongSock){
            strongSock->onRemoteErr(ex);
        }
    }
}

void ProxyChannel::computeDataGenerateSeed(){
    auto ms = _manageResTicker.elapsedTime();
    if (ms < 500) {
        //最少500ms计算一次网速
        return;
    }
    //计算数据产生速率
    _bytesAckPerSec = _totalAckBytes * 1000 / ms;
    _totalAckBytes = 0;
    _manageResTicker.resetTime();
    onChangePktSize();
}

void ProxyChannel::onManager(){
    static float bufferFlushTime = mINI::Instance()[Config::Proxy::kBufferFlushTime];
    if(_flushTicker.elapsedTime() >= bufferFlushTime * 1000 && _sendBuf.size()){
        //每bufferFlushTime秒刷新清空缓存
        realSend("",true);
    }
    if(_manageResTicker.elapsedTime() > 2000){
        manageResponse(CODE_TIMEOUT,"ProxyChannel timeout",false);
        computeDataGenerateSeed();
    }
}

void ProxyChannel::onChangePktSize(){
    if(!_bytesAckPerSec){
        return;
    }
    static int minPktSize = mINI::Instance()[Config::Proxy::kMinPktSize];
    static int maxPktSize = mINI::Instance()[Config::Proxy::kMaxPktSize];
    static float bufferFlushTime = mINI::Instance()[Config::Proxy::kBufferFlushTime];
    static float maxNoAckBufferSec = mINI::Instance()[Config::Proxy::kMaxBufferSec];
    static float minNoAckBufferSize = mINI::Instance()[Config::Proxy::kMinNoAckBufferSize];
    //包大小为半秒网速
    _pktSize = _bytesAckPerSec * bufferFlushTime;
    //包大小不能超过上下限
    _pktSize = MAX(minPktSize * 1024, MIN(maxPktSize * 1024, _pktSize));
    //最多允许为Ack的数据量
    _maxNoAckBytes = maxNoAckBufferSec * _bytesAckPerSec;
    //设置下限制
    _maxNoAckBytes = MAX(minNoAckBufferSize * 1024, _maxNoAckBytes);
    //发送网速事件关闭
    NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastSendSpeed, (string &) _dst_uuid, (uint64_t &) _bytesAckPerSec);
}

bool ProxyChannel::isEnableAck() const{
    return _enable_send_ack;
}

bool ProxyChannel::enableAck(bool flag) {
    if (flag) {
        for (auto &cb : _ack_list) {
            cb();
        }
        _ack_list.clear();
    }

    if (_enable_send_ack != flag) {
        _enable_send_ack = flag;
        InfoL << "enable recv data from remote:" << flag;
        return true;
    }
    return false;
}

void ProxyChannel::pushAckCB(function<void()> ack) {
    if (_enable_send_ack) {
        ack();
    } else {
        _ack_list.emplace_back(std::move(ack));
        WarnL << "block ack size:" << _ack_list.size();
    }
}

} /* namespace Proxy */
