/*
 * ProxyJumpSession.h
 *
 *  Created on: 2017年6月12日
 *      Author: xzl
 */

#ifndef SESSION_TCPPROXYJUMPSESSION_H_
#define SESSION_TCPPROXYJUMPSESSION_H_

#include "Network/TcpSession.h"
#include "Util/SSLBox.h"
#include "Util/TimeTicker.h"
#include "json/json.h"
#include "ProxyProtocol.h"

using namespace Json;
using namespace toolkit;

namespace Proxy {


class ProxyJumpSession: public TcpSession , public  ProxyProtocol{
public:
    typedef std::shared_ptr<ProxyJumpSession> Ptr;
    ProxyJumpSession(const Socket::Ptr &sock);
    virtual ~ProxyJumpSession();
    virtual void onRecv(const Buffer::Ptr &) override;
    virtual void onError(const SockException &err) override {};
    virtual void onManager() override;
private:
    typedef void (ProxyJumpSession::*onHandleRequest)(uint64_t seq, const Value &obj,const string &body);
    virtual bool onProcessRequest(const string &cmd,uint64_t seq,const Value &obj,const string &body) override;
    virtual int  onSendData(const string &data) override;
    Ticker _beatTicker;
};

} /* namespace Proxy */

#endif /* SESSION_TCPPROXYJUMPSESSION_H_ */
