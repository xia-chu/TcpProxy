/*
 * TcpServerImp.h
 *
 *  Created on: 2017年6月15日
 *      Author: xzl
 */

#ifndef PROXY_TCPSERVERIMP_H_
#define PROXY_TCPSERVERIMP_H_

#include <memory>
#include <exception>
#include <functional>
#include "Network/Socket.h"
#include "Network/TcpServer.h"
#include "Util/util.h"
#include "Util/uv_errno.h"
#include "Util/logger.h"
#include "Poller/Timer.h"

using namespace std;
using namespace toolkit;

namespace Proxy {

class TcpServerImp : public TcpServer{
public:
    typedef std::shared_ptr<TcpServerImp> Ptr;
    TcpServerImp(void *ctx) : TcpServer(EventPoller::Instance().shared_from_this()) {
        contex = ctx;
        testSockConnect.reset(new Socket(EventPoller::Instance().shared_from_this()));
    }
    ~TcpServerImp() {}

    void *getContex(){
        return contex;
    }

    template <typename SessionType>
    uint16_t start(uint16_t port, const std::string& host = "0.0.0.0", uint32_t backlog = 1024) {
        TcpServer::start<SessionType>(port, host, backlog);
        port = getPort();

        testSockConnect->connect("127.0.0.1", port, [](const SockException &err) {
            InfoL << "连接本地端口成功！";
        });
        weak_ptr<TcpServerImp> weakSelf = dynamic_pointer_cast<TcpServerImp>(this->shared_from_this());
        testSockConnect->setOnErr([weakSelf, port, host, backlog, this](const SockException &err) {
            WarnL << "TCP 监听被系统回收: " << err.getErrCode() << " " << err.what();
            EventPoller::Instance().doDelayTask(100, [weakSelf, port, host, backlog]() {
                auto strongSelf = weakSelf.lock();
                if (strongSelf) {
                    try {
                        strongSelf->start<SessionType>(port, host, backlog);
                    } catch (std::exception &ex) {
                        ErrorL << "重新监听失败：" << ex.what();
                    }
                }
                return 0;
            });
        });
        return port;
    }

protected:
    Socket::Ptr onBeforeAcceptConnection(const EventPoller::Ptr &poller) override {
        /**
         * 服务器器模型socket是线程安全的，所以为了提高性能，关闭互斥锁
         */
        return std::make_shared<Socket>(EventPoller::Instance().shared_from_this(),false);
    }

    TcpServer::Ptr onCreatServer(const EventPoller::Ptr &poller) override{
        return nullptr;
    }

private:
    Session::Ptr onAcceptConnection(const Socket::Ptr &sock) override {
        if (testSockConnect->get_local_port() == 0 ||
            (sock->get_peer_ip() == testSockConnect->get_local_ip() &&
             sock->get_peer_port() == testSockConnect->get_local_port())) {
            //是clientSock链接
            testSockAccept = sock;
            InfoL << "获取到本地连接！";
            // return;
        }
        return TcpServer::onAcceptConnection(sock);
    }
private:
    void *contex;
    Socket::Ptr testSockConnect;
    Socket::Ptr testSockAccept;
};

} /* namespace Proxy */

#endif /* PROXY_TCPSERVERIMP_H_ */
