/*
 * ProxyBinderSession.cpp
 *
 *  Created on: 2017年6月15日
 *      Author: xzl
 */

#include "ProxyBinderSession.h"
#include "Network/sockutil.h"
using namespace toolkit;

namespace Proxy {

ProxyBinderSession::ProxyBinderSession(const Socket::Ptr &sock):TcpSession(sock){
	static int timeoutSec = mINI::Instance()[Config::Session::kTimeoutSec];
	getSock()->setSendTimeOutSecond(timeoutSec);
	DebugP(this);
}

void ProxyBinderSession::onStart(){
	_toSock = _contex->_terminal->obtain(_contex->_toUuid);
	weak_ptr<ProxyBinderSession> weakSelf = dynamic_pointer_cast<ProxyBinderSession>(shared_from_this());
    _toSock->setOnRemoteConnect([weakSelf](const SockException &ex) {
        //WarnL << "ProxySock onConnect:" << ex.getErrCode() << " " << ex.what();
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        strongSelf->_connected = !(ex.operator bool());
        if (!strongSelf->_connected) {
            strongSelf->onSocketErr(ex);
            strongSelf->shutdown();
        } else {
            strongSelf->flushData();
        }
    });
    _toSock->setOnRemoteErr([weakSelf](const SockException &ex) {
        //WarnL << "ProxySock onErr:" << ex.getErrCode() << " " << ex.what();
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        strongSelf->onSocketErr(ex);
        strongSelf->shutdown();
    });
    _toSock->setOnRemoteRecv([weakSelf](const string &data) {
        Status::Instance().addCountBindServer(false, data.size());
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        strongSelf->SockSender::send(data);
        if (strongSelf->isSocketBusy()) {
            //本地客户端接收数据不过来，设置远端TCP客户端不要再接收服务器数据
            strongSelf->_toSock->enableRecvFromRemote(false);
        }
    });

    getSock()->setOnFlush([weakSelf]() {
        //本地socket发送缓存清空了，那么设置远端TCP客户端允许接收数据
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return false;
        }
        //设置对端是否允许接收数据
        strongSelf->_toSock->enableRecvFromRemote(true);
        return true;
    });

    _toSock->setOnRemoteSendable([weakSelf](bool enabled) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        strongSelf->getSock()->enableRecv(enabled);
    });

    _toSock->connectRemote(_contex->_toUrl, _contex->_toPort);
}

void ProxyBinderSession::onSocketErr(const SockException &ex){
	_contex->onSocketErr(ex);
}

void ProxyBinderSession::flushData() {
    _toSock->sendRemote(_dataStream);
	_dataStream.clear();
}

void ProxyBinderSession::onRecv(const Buffer::Ptr &buf) {
    Status::Instance().addCountBindServer(true, buf->size());
    if(_connected){
        _toSock->sendRemote(string(buf->data(), buf->size()));
	}else{
		_dataStream.append(buf->data(),buf->size());
	}
}

void ProxyBinderSession::attachServer(const Server &srv) {
	TcpServerImp *server = (TcpServerImp *)(&srv);
	_contex = (ProxyBinder *)server->getContex();
	onStart();
}

///////////////ProxyBinder///////////////

ProxyBinder::ProxyBinder(const ProxyTerminalInterface::Ptr& terminal): _terminal(terminal){
}

void ProxyBinder::bind(const string& touuid, uint16_t port,const string& url) {
	_toUuid = touuid;
	_toPort = port;
	_toUrl = url;
    std::weak_ptr<ProxyBinder> weakSelf = shared_from_this();
    _terminal->bind(touuid,[weakSelf](int code,const string &msg,const Value &obj,const string &body){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        if(strongSelf->_bindResult){
            strongSelf->_bindResult(code,msg,obj,body);
        }
    });
}
uint16_t ProxyBinder::start(uint16_t port, const string& ip) {
	_server.reset(new TcpServerImp(this));
	return _server->start<ProxyBinderSession>(port,ip);
}

} /* namespace Proxy */
