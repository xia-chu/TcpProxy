/*
 * ProxyBinderSession.h
 *
 *  Created on: 2017年6月15日
 *      Author: xzl
 */

#ifndef PROXY_PROXYBINDERSESSION_H_
#define PROXY_PROXYBINDERSESSION_H_

#include "Network/TcpSession.h"
#include "TcpServerImp.h"
#include "ProxyChannel.h"
#include "ProxyTerminal.h"

using namespace toolkit;

namespace Proxy {

class ProxyBinderSession;

class ProxyBinder : public std::enable_shared_from_this<ProxyBinder>{
public:
    typedef std::shared_ptr<ProxyBinder> Ptr;
    friend class ProxyBinderSession;
    ProxyBinder(const ProxyTerminalInterface::Ptr &terminal);
    virtual ~ProxyBinder(){}

    void bind(const string &touuid,uint16_t port,const string &url="127.0.0.1");
    uint16_t start(uint16_t port,const string &ip="0.0.0.0");

    void setBindResultCB(const onResponse& bindResult) {
        _bindResult = bindResult;
    }

    void setSocketErrCB(const Socket::onErrCB& socketErr) {
        _socketErr = socketErr;
    }

private:
    void onSocketErr(const SockException &ex){
        if(_socketErr){
            _socketErr(ex);
        }
    }

private:
    TcpServerImp::Ptr _server;
    ProxyTerminalInterface::Ptr _terminal;
    string _toUuid;
    uint16_t _toPort = 0;
    string _toUrl;
    onResponse _bindResult;
    Socket::onErrCB _socketErr;
};

class ProxyBinderSession : public TcpSession {
public:
    typedef std::shared_ptr<ProxyBinderSession> Ptr;
    ProxyBinderSession(const Socket::Ptr &sock);
    virtual ~ProxyBinderSession(){};
    virtual void onRecv(const Buffer::Ptr &) override;
    virtual void onError(const SockException &err) override{};
    virtual void onManager() override {};
    virtual void attachServer(const Server &server) override;

private:
    void onStart();
    void flushData();
    void onSocketErr(const SockException &ex);

private:
    ProxyBinder *_contex;
    ProxySocket::Ptr _toSock;
    bool _connected = false;
    string _dataStream;

};

} /* namespace Proxy */

#endif /* PROXY_PROXYBINDERSESSION_H_ */
