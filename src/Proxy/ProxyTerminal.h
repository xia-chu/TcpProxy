/*
 * ProxySocket.h
 *
 *  Created on: 2017年6月14日
 *      Author: xzl
 */

#ifndef PROXY_PROXYTERMINAL_H_
#define PROXY_PROXYTERMINAL_H_

#include "ProxyClient.h"
#include "ProxyProtocol.h"
#include "ProxyChannel.h"

namespace Proxy {

//实现TCP代理协议的相关接口
class ProxyTerminalInterface {
public:
    typedef std::shared_ptr<ProxyTerminalInterface> Ptr;
    ProxyTerminalInterface(){}
    virtual ~ProxyTerminalInterface(){}
    virtual ProxySocket::Ptr obtain(const string &touuid) = 0;
    virtual void bind(const string &dst,const onResponse &cb) = 0;
};

//实现TCP代理协议的对象
class ProxyTerminalBase: public ProxyTerminalInterface , public std::enable_shared_from_this<ProxyTerminalBase>{
public:
    typedef std::shared_ptr<ProxyTerminalBase> Ptr;

    ProxyTerminalBase(const std::shared_ptr<ProxyClientInterface> &connection) : _connection(connection) {
        _connection->setOnTransfer([this](const string &from, const string &body, onResponse &onRes) {
            onTransfer(from, body, onRes);
        });
    }

    ~ProxyTerminalBase() override{
        _connection->setOnTransfer(nullptr);
    }

    void bind(const string &dst, const onResponse &cb) override {
        auto chn = getChannel(dst);
        chn->bind(cb);
    }

    ProxySocket::Ptr obtain(const string &touuid) override {
        ProxySocket::Ptr ret(new ProxySocket(getChannel(touuid)), [](ProxySocket *ptr) {
            //ProxySocket对象需要放在主线程释放
            EventPoller::Instance().async([ptr]() {
                delete ptr;
            });
        });
        return ret;
    }

    void clearChannel() {
        _channelMap.clear();
    }

    template<typename T>
    std::shared_ptr<T> getConnection() const {
        return dynamic_pointer_cast<T>(_connection);
    }

private:
    ProxyChannel::Ptr &getChannel(const string &uuid) {
        auto &channel = _channelMap[uuid];
        if (!_timer && _channelMap.size() == 1) {
            //需要创建定时器
            weak_ptr<ProxyTerminalBase> weakSelf = dynamic_pointer_cast<ProxyTerminalBase>(shared_from_this());
            _timer.reset(new Timer(0.2, [weakSelf]() {
                auto strongSelf = weakSelf.lock();
                if (strongSelf) {
                    strongSelf->onManagerChannel();
                }
                return strongSelf.operator bool();
            }, EventPoller::Instance().shared_from_this()));
            DebugL << "start manager channel...";
        }
        if (channel) {
            return channel;
        }
        channel.reset(new ProxyChannel(uuid));
        weak_ptr<ProxyTerminalBase> weakSelf = dynamic_pointer_cast<ProxyTerminalBase>(shared_from_this());
        channel->setOnSend([weakSelf, uuid](const string &data) {
            auto strongSelf = weakSelf.lock();
            if (strongSelf) {
                return strongSelf->onSendData(uuid, data);
            }
            return -1;
        });
        return channel;
    }

    void onTransfer(const string &from, const string &body, onResponse &onRes) {
        try  {
            auto &channel = getChannel(from);
            channel->pushAckCB([onRes]() {
                onRes(CODE_SUCCESS, "", objectValue, "");
            });
            channel->onRecvData(body.data(), body.size());
        } catch (std::exception &ex) {
            onChannelErr(from, CODE_OTHER, ex.what());
            onRes(CODE_OTHER, ex.what(), objectValue, "");
        }
    }

    int onSendData(const string &to, const string &data) {
        auto &channel = getChannel(to);
        int size = data.size();
        channel->addSendCount(size);
        Status::Instance().addBytesTransfer(size);

        weak_ptr<ProxyChannel> weakChannel = channel;
        weak_ptr<ProxyTerminalBase> weakSelf = dynamic_pointer_cast<ProxyTerminalBase>(shared_from_this());
        _connection->transfer(to, data, [weakSelf, to, weakChannel, size](int code, const string &msg, const Value &obj, const string &body) {
            if (code == CODE_SUCCESS) {
                Status::Instance().addBytesAck(size);
            } else {
                Status::Instance().addBytesAckFailed(size);
            }
            auto strongChannel = weakChannel.lock();
            if (!strongChannel) {
                Status::Instance().clearNoAck();
            }

            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return;
            }
            if (code == CODE_SUCCESS) {
                if (strongChannel) {
                    strongChannel->onSendSuccess(size);
                }
            } else {
                //传输数据失败
                strongSelf->onChannelErr(to, code, msg);
            }
        });
        return data.size();
    }

    void onChannelErr(const string &channelid,int code,const string &errMsg){
        //通道发生错误
        auto it = _channelMap.find(channelid);
        if (it != _channelMap.end()) {
            auto chn = it->second;
            chn->onErr(code, errMsg);
            _channelMap.erase(it);
            WarnL << "erase channel[" << channelid << "]:" << code << " " << errMsg;
        }
        if (_channelMap.empty() && _timer) {
            //没有任何通道需要管理，删除定时器
            _timer.reset();
            DebugL << "stop manager channel!";
        }
    }

    void onManagerChannel(){
        for (auto it = _channelMap.begin(); it != _channelMap.end();) {
            if (it->second->expired()) {
                it = _channelMap.erase(it);
                continue;
            }
            auto chn = it->second;
            chn->onManager();
            ++it;
        }
        if (_channelMap.empty() && _timer) {
            //没有任何通道需要管理，删除定时器
            _timer.reset();
            DebugL << "stop manager channel!";
        }
    }

private:
    ProxyClientInterface::Ptr _connection;
    unordered_map<string,ProxyChannel::Ptr> _channelMap;
    std::shared_ptr<Timer> _timer;
};

//实现TCP代理以及信令客户端的对象
class ProxyTerminal : public ProxyTerminalBase {
public:
    typedef std::shared_ptr<ProxyTerminal> Ptr;
    ProxyTerminal() : ProxyTerminalBase(std::make_shared<ProxyClient>()) {}
    ~ProxyTerminal() override {}

    virtual void login(const string &url, uint16_t port, const string &uuid, const Value &userInfo = Value(objectValue), bool enabelSsl = true) {
        ProxyClient::Ptr connection = getConnection<ProxyClient>();
        weak_ptr<ProxyTerminal> weakSelf = dynamic_pointer_cast<ProxyTerminal>(shared_from_this());
        connection->setOnLogin([weakSelf](int code, const string &msg) {
            auto strongSelf = weakSelf.lock();
            if (strongSelf) {
                strongSelf->onLoginResult(code, msg);
            }
        });
        connection->setOnShutdown([weakSelf](const SockException &ex) {
            auto strongSelf = weakSelf.lock();
            if (strongSelf) {
                strongSelf->onShutdown(ex);
            }
        });
        connection->login(url, port, uuid, userInfo, enabelSsl);
    }

    virtual void logout() {
        ProxyClient::Ptr connection = getConnection<ProxyClient>();
        connection->logout();
        clearChannel();
    }

    int status() const {
        ProxyClient::Ptr connection = getConnection<ProxyClient>();
        return connection->status();
    }

    void message(const string &to, const Value &obj, const string &body, const onResponse &cb) {
        getConnection<ProxyClient>()->message(to, obj, body, cb);
    }

    void joinRoom(const string &room_id, const onResponse &cb) {
        getConnection<ProxyClient>()->joinRoom(room_id, cb);
    }

    void exitRoom(const string &room_id, const onResponse &cb) {
        getConnection<ProxyClient>()->exitRoom(room_id, cb);
    }

    void broadcastRoom(const string &room_id, const string &body, const onResponse &cb) {
        getConnection<ProxyClient>()->broadcastRoom(room_id, body, cb);
    }

    void setOnLogin(const Proxy::onLoginResult &cb) {
        _onLogin = cb;
    }

    void setOnShutdown(const Proxy::onShutdown &cb) {
        _onShutdown = cb;
    }

protected:
    virtual void onShutdown(const SockException &ex) {
        if (_onShutdown) {
            _onShutdown(ex);
        }
    }

    virtual void onLoginResult(int code, const string &msg) {
        if (_onLogin) {
            _onLogin(code, msg);
        }
    }

private:
    Proxy::onLoginResult _onLogin;
    Proxy::onShutdown _onShutdown;
};

} /* namespace Proxy */
#endif /* PROXY_PROXYTERMINAL_H_ */
