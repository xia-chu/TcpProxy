/*
 * ProxyChannel.h
 *
 *  Created on: 2017年6月15日
 *      Author: xzl
 */

#ifndef PROXY_PROXYCHANNEL_H_
#define PROXY_PROXYCHANNEL_H_

#include <memory>
#include <iostream>
#include <iomanip>
#include <functional>
#include "Config.h"
#include "ProxyProtocol.h"
#include "Poller/Timer.h"
#include "Network/Socket.h"
#include "Util/NoticeCenter.h"

using namespace std;
using namespace toolkit;

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b) )
#endif //MAX

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b) )
#endif //MIN

namespace Proxy {

typedef std::shared_ptr<int> SockID;
class ProxyChannel;

//对ProxyChannel对象API的封装
class ProxySocket : public std::enable_shared_from_this<ProxySocket>{
public:
	friend class ProxyChannel;
	typedef std::shared_ptr<ProxySocket> Ptr;
	typedef function<void(const string &data)> onRemoteRecv;
	typedef function<void(bool enabled)> onRemoteSendable;

	ProxySocket(const std::shared_ptr<ProxyChannel> &channel);
	~ProxySocket();

    //让远端连接目标地址
    void connectRemote(const string &url, uint16_t port, int timeout = 5);
	//控制远端发送数据
    int sendRemote(const string &data);
    //是否允许从远端接收数据
    void enableRecvFromRemote(bool flag);

    //监听远端建立连接的结果
    void setOnRemoteConnect(const Socket::onErrCB &cb);
    //监听远端socket断开事件
    void setOnRemoteErr(const Socket::onErrCB &cb);
    //监听远端接收到数据事件
    void setOnRemoteRecv(const ProxySocket::onRemoteRecv &cb);
    //监听是否允许发送数据到远端
    void setOnRemoteSendable(const onRemoteSendable &cb);

private:
	void onRemoteConnect(const SockException &ex);
	void onRemoteErr(const SockException &ex);
	void onRemoteRead(const string &data);
	void setSockid(const SockID& sockid);
	void enableSendRemote(bool enabled);

private:
	std::shared_ptr<ProxyChannel> _channel;
	SockID _sockid;
	Socket::onErrCB _onRemoteErr;
	Socket::onErrCB _onRemoteConnect;
	onRemoteSendable _onRemoteSendable;
	ProxySocket::onRemoteRecv _onRemoteRecv;
};

//实现TCP代理协议，根据信令操作socket
class ProxyChannel: public ProxyProtocol , public std::enable_shared_from_this<ProxyChannel> {
public:
	friend class ProxySocket;
	friend class ProxyTerminalBase;
	typedef std::shared_ptr<ProxyChannel> Ptr;
	typedef function<int(const string &data)> onSend;

	ProxyChannel(const string &dst_uuid);
	virtual ~ProxyChannel();
	void setOnSend(const onSend& onSend);
	bool expired();
	void bind(const onResponse &cb);
	void addSendCount(int size);

protected:
	virtual bool onProcessRequest(const string &cmd,uint64_t seq, const Value &obj,const string &body) override;
	virtual int  onSendData(const string &data) override;
	int realSend(const string &data,bool flush = false);

private:
	void slave_handleRequest_connect(uint64_t seq, const Value &obj,const string &body);
	void slave_handleRequest_send(uint64_t seq, const Value &obj,const string &body);
	void slave_handleRequest_close(uint64_t seq, const Value &obj,const string &body);
	void slave_handleRequest_bind(uint64_t seq, const Value &obj,const string &body);

	typedef function<void(const SockException &ex,const SockID &sockid)> onNewSock;
	void master_handleRequest_onRecv(uint64_t seq, const Value &obj,const string &body);
	void master_handleRequest_onErr(uint64_t seq, const Value &obj,const string &body);
	void master_requestOpenSocket(const string &url ,uint16_t port,const onNewSock &cb,int timeout = 5);
	void master_requestCloseSocket(int sockid);
	void master_requestSendData(int sockid,const string &data);
	void master_attachEvent(const ProxySocket::Ptr &obj);
	void master_detachEvent(int sockid);
	void master_onSockData(int sockid,const string &data);
	void master_onSockErr(int sockid,const SockException &ex);

	void onErr(int code,const string &errMsg);
	void onSendSuccess(int count);
	void sendPacket(const string &data);
	//设置是否允许产生数据(未ack的数据太多了)
	bool enableGenerateData(bool enabled);
	bool isEnableGenerateData() const;
    //设置是否允许立即发送ack(本地socket接收能力不足)
    bool enableAck(bool flag);
	bool isEnableAck() const;
	void pushAckCB(function<void()> ack);

    void detachAllSock();
	void onManager();
	void onChangePktSize();
	void computeDataGenerateSeed();

private:
    //是否允许发送ack包
    bool _enable_send_ack = true;
    //如果拥塞数据太多，则需要关闭上游数据的产生并且停止转发
    bool _enableGenerateData = true;
    //自增长的socket id
    int _slaveCurrentSockId = 0;
    //包大小
    int _pktSize = 0;
    //最大允许未确认的数据大小
    int _maxNoAckBytes = 0;
    //拥塞的数据字节数
    int _noAckBytes = 0;
    //计算经过确认的数据速率
    uint64_t _bytesAckPerSec = 0;
    uint64_t _totalAckBytes = 0;
    //目标id
    string _dst_uuid;
    //拥塞数据太多，这时应该缓存上游数据并且停止转发
    string _sendBuf;
    //数据发送回调
    onSend _onSend;
    //延迟发送的ack包
    list<function<void()> > _ack_list;
    //salve端本地socket
    unordered_map<int, Socket::Ptr> _slaveSockMap;
    //虚拟socket
    unordered_map<int, std::weak_ptr<ProxySocket> > _masterListenerMap;
    //200 ms清空一下延时发送缓存
    Ticker _flushTicker;
    //ack回复超时计时器
    Ticker _manageResTicker;
};

} /* namespace Proxy */
#endif /* PROXY_PROXYCHANNEL_H_ */
