/*
 * ProxyClient.h
 *
 *  Created on: 2017年6月14日
 *      Author: xzl
 */

#ifndef PROXY_PROXYCLIENT_H_
#define PROXY_PROXYCLIENT_H_

#include <memory>
#include "json/json.h"
#include "Network/TcpClient.h"
#include "ProxyProtocol.h"
#include "Util/SSLBox.h"
#include "Util/onceToken.h"

using namespace Json;
using namespace toolkit;

namespace Proxy {

typedef function<void(int code,const string &msg)> onLoginResult;
typedef function<void(const string &from,const string &body,onResponse &onRes)> onTransfer;
typedef function<void(const SockException &ex)> onShutdown;

//TCP代理传输协议序列化反序列化的接口
class ProxyClientInterface{
public:
    typedef std::shared_ptr<ProxyClientInterface> Ptr;
    ProxyClientInterface(){};
	virtual ~ProxyClientInterface(){}
	virtual void setOnTransfer(const onTransfer& onTransfer) = 0;
    virtual void transfer(const string &to,const string &body,const onResponse &cb) = 0;
};

//TCP代理传输协议序列化反序列化的对象
class ProxyCientBase : public ProxyClientInterface, public ProxyProtocol{
public:
    ProxyCientBase() : ProxyProtocol(TYPE_CLIENT) {};
    ~ProxyCientBase() override {};

	void transfer(const string &to,const string &body,const onResponse &cb) override;
	void setOnTransfer(const onTransfer& onTransfer) override;

protected:
	virtual std::shared_ptr<ProxyCientBase> getReference()  = 0;
	bool onProcessRequest(const string& cmd,uint64_t seq, const Value& obj, const string& body) override;

private:
	void handleRequest_transfer(const string &cmd,uint64_t seq, const Value& obj,const string& body);

private:
	onTransfer _onTransfer;
};

//支持更多命令，用户TCP长连接信令协议
class ProxyClient: public TcpClient , public ProxyCientBase {
public:
	typedef std::shared_ptr<ProxyClient> Ptr;

	ProxyClient();
	~ProxyClient() override;
	void login(const string &url,uint16_t port,const string &uuid,const Value &userInfo,bool enableSsl = true);
	void logout();
	int status() const;
	void transfer(const string &to,const string &body,const onResponse &cb) override ;
    void message(const string &to, const Value &obj, const string &body,const onResponse &cb);
    void joinRoom(const string &room_id,const onResponse &cb);
    void exitRoom(const string &room_id,const onResponse &cb);
    void broadcastRoom(const string &room_id,const string &body,const onResponse &cb);
    void setOnLogin(const onLoginResult& onLogin);
	void setOnShutdown(const onShutdown& onShutdown);
	std::shared_ptr<ProxyCientBase> getReference() override;

protected:
	void onConnect(const SockException &ex) override;
	void onRecv(const Buffer::Ptr &pBuf) override;
	void onErr(const SockException &ex) override;

	bool onProcessRequest(const string &cmd,uint64_t seq,const Value &obj,const string &body) override;
	int  onSendData(const string &data) override;
	void disconnect(const SockException &ex = SockException());

private:
	void handleRequest_conflict(const string &cmd, uint64_t seq, const Value &obj,const string &body);
    void handleRequest_message(const string &cmd, uint64_t seq, const Value& obj,const string& body);
    void handleRequest_onJoinRoom(const string &cmd, uint64_t seq, const Value& obj,const string& body);
    void handleRequest_onExitRoom(const string &cmd, uint64_t seq, const Value& obj,const string& body);
    void handleRequest_onBroadcast(const string &cmd, uint64_t seq, const Value& obj,const string& body);
    void handleRequest_onRoomEvent(const string &cmd, uint64_t seq, const Value& obj,const string& body,const char *event);

    void sendRequest_login();
	void sendBeat();
	void onManager() override;

    void onDecData(const char *data, uint32_t len);
    void onEncData(const char *data, uint32_t len);
	void makeSslBox();

private:
    std::shared_ptr<SSL_Box> _sslBox;
	string _uuid;
	Value _userInfo;
	onLoginResult _onLogin;
	onShutdown _onShutdown;
	STATUS_CODE _status = STATUS_LOGOUTED;
    bool _enabelSsl = true;
    string _server_url;
};

} /* namespace Proxy */
#endif /* PROXY_PROXYCLIENT_H_ */
