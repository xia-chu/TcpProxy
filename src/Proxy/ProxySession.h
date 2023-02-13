/*
 * TcpProxySession.h
 *
 *  Created on: 2017年6月12日
 *      Author: xzl
 */

#ifndef SESSION_TCPPROXYSESSION_H_
#define SESSION_TCPPROXYSESSION_H_

#include <unordered_set>
#include "Network/Session.h"
#include "Util/SSLBox.h"
#include "Util/TimeTicker.h"
#include "json/json.h"
#include "ProxyProtocol.h"

using namespace Json;
using namespace toolkit;

namespace Proxy {

class ProxySession: public Session, public ProxyProtocol {
public:
    typedef std::shared_ptr<ProxySession> Ptr;
    typedef function<void (const string &err)> AuthInvoker;
    typedef function<void (int code, const string &msg,const Value &obj,const string &body)> ResponseInvoker;

    static uint64_t getSessionCount();
    ProxySession(const Socket::Ptr &sock);
    ~ProxySession() override;

    virtual void onRecv(const Buffer::Ptr &) override;
    virtual void onError(const SockException &err) override;
    virtual void onManager() override;

    const Value& getLoginInfo() const;
    const string& getSessionUuid() const;
    const string& getSessionAppId() const;

    /**
     * 线程安全的
     */
    void sendRequestFromOther(const string &cmd, const string &from, const Value &obj, const string &body, const onResponse &cb);

    /**
     * 线程安全的
     */
    void onConflict(const string &ip);

private:
    virtual bool onProcessRequest(const string &cmd,uint64_t seq,const Value &obj,const string &body) override;
    virtual int  onSendData(const string &data) override;

    void handleRequest_login(const string &cmd,uint64_t seq, const Value &obj,const string &body);
    void handleRequest_transfer(const string &cmd,uint64_t seq, const Value &obj,const string &body);
    void handleRequest_joinRoom(const string &cmd,uint64_t seq, const Value &obj,const string &body);
    void handleRequest_exitRoom(const string &cmd,uint64_t seq, const Value &obj,const string &body);
    void handleRequest_broadcastRoom(const string &cmd,uint64_t seq, const Value &obj,const string &body);
    void handleRequest_beat(const string &cmd,uint64_t seq, const Value &obj,const string &body);
    bool broadcastRoomEvent(const string &event,const string &room_id,const Value &obj, const string &body,bool emitEvent = true);

    void exitRoom(const string &room_id,bool emitEvent = true);
    void checkState(const string &cmd,STATUS_CODE state);

    //登录成功的回调
    void onAuthSuccess(int seq, const string &appId, const string &uuid, const Value &obj);
    void onLogin();
    void onLogout();

private:
    //应用id
    string _app_id;
    //会话唯一标识符
    string _user_id;
    //用户登录的obj对象
    Value _login_info;
    STATUS_CODE _status = STATUS_LOGOUTED;
    unordered_set<string> _joined_rooms;
};

class UserManager {
public:
    typedef std::shared_ptr<UserManager> Ptr;
    ~UserManager(){};
    static UserManager &Instance(const string &appId);
    ProxySession::Ptr get(const string &user_name);
    void add(const string &user_name,const ProxySession::Ptr &ptr);
    bool del(const string &user_name,ProxySession *obj);

private:
    UserManager(const string &appId);

private:
    string _appId;
    unordered_map<string,weak_ptr<ProxySession> > _mapSession;
    recursive_mutex _mtxSession;
};

class RoomManager{
public:
    typedef std::shared_ptr<RoomManager> Ptr;
    ~RoomManager(){};
    static RoomManager &Instance(const string &appId);

    void joinRoom(const string &room_id, const string &user_name, const ProxySession::Ptr &ptr,const ProxySession::AuthInvoker &invoker);
    bool exitRoom(const unordered_set<string> &room_ids, const string &user_name, const ProxySession::Ptr &obj,bool emitEvent);
    bool forEachRoomMember(const string &room_id,const string &user_name,const function<void(ProxySession::Ptr &ptr)> &callback);

private:
    void onAddRoom(const string &room_id);
    void onRemoveRoom(const string &room_id);
    RoomManager(const string &appId);

private:
    string _appId;
    unordered_map<string,unordered_map<string,weak_ptr<ProxySession> > > _mapRoom;
    recursive_mutex _mtxRoom;
};


} /* namespace Proxy */
#endif /* SESSION_TCPPROXYSESSION_H_ */
