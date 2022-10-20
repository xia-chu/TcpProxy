/*
 * ProxyProtocol.h
 *
 *  Created on: 2017年6月14日
 *      Author: xzl
 */

#ifndef PROXYPROTOCOL_H_
#define PROXYPROTOCOL_H_

#include <unordered_map>
#include "Util/TimeTicker.h"
#include "json/json.h"
#include "Config.h"
#include "HttpRequestSplitter.h"
#include "Status.h"

using namespace Json;
using namespace toolkit;

namespace Proxy {

/*
 * 协议描述：
 * ------------------------------------------------------
 * [Header:JsonObject]
 * \r\n\r\n
 * [Body:RawData]
 * 协议头与协议负载字节有两个回车符分隔
 * 次协议类似http，但是协议头是json
 * ------------------------------------------------------
 *
 * 协议头(Header)描述：
 * ------------------------------------------------------
 * 请求类型的头：
 * {
 *   "type":"request",		// 请求类型
 *	 "seq":0,  				// 请求序号
 *	 "cmd":"transfer",		// 命令名称
 *	 "body_len":len,        //body长度
 *	 "obj":{}				// 请求对象
 * }
 *
 * 回复类型的头：
 * {
 * 	"type":"response",      // 回复类型
 * 	"seq": 0,			    // 回复序号（对应请求序号）
 * 	"body_len":len,         //body长度
 * 	"code":0,				// 回复代码（0:成功，其他：错误）
 * 	"msg":"success",        // 回复文本（一些错误提示）
 * 	"obj":{}					// 回复对象
 * }
 * ------------------------------------------------------
 */
typedef enum {
    CODE_SUCCESS = 0,
    CODE_OTHER = -1,
    CODE_TIMEOUT = -2,
    CODE_AUTHFAILED = -3,
    CODE_UNSUPPORT = -4,
    CODE_NOTFOUND = -5,
    CODE_JUMP = -6,//服务器跳转
    CODE_BUSY = -7,//服务器忙
    CODE_NETWORK = -8,//网络错误
    CODE_GATE = -9,//网关错误
    CODE_BAD_REQUEST = -10,//非法的请求
    CODE_CONFLICT = -100, //被挤占登录
    CODE_BAD_LOGIN = -101,//正在登陆或者已经登录了
} PROXY_CODE;


typedef enum {
    STATUS_LOGOUTED = 0,
    STATUS_LOGINING,
    STATUS_LOGINED
} STATUS_CODE;

typedef function<void(int code, const string &msg, const Value &obj, const string &body)> onResponse;

//回复绑定者
class ResponseBinder {
public:
    typedef std::shared_ptr<ResponseBinder> Ptr;

    template<typename FUN>
    ResponseBinder(FUN &&fun) : _invoker(fun) {}
    ~ResponseBinder() {}

    void operator()(int code, const string &msg, const Value &obj, const string &body) {
        if (_invoker) {
            try {
                _invoker(code, msg, obj, body);
            } catch (std::exception &ex) {
                WarnL << ex.what();
            }
        }
    }

    bool expired() const {
        //回复超时时间
        static float sec = mINI::Instance()[Config::Proxy::kResTimeoutSec];
        return _ticker.elapsedTime() > sec * 1000;
    }

private:
    onResponse _invoker;
    mutable Ticker _ticker;
};

class ProxyProtocol : public ::HttpRequestSplitter {
public:
    enum SubClassType {
        TYPE_CLIENT = 0,
        TYPE_SERVER = 1,
        TYPE_CHANNEL = 2,
    };

    ProxyProtocol(SubClassType type);
    virtual ~ProxyProtocol();

    void sendResponse(uint64_t seq, int code, const string &msg, const Value &obj, const string &body = "");
    void sendRequest(const string &cmd, const Value &obj, const string &body, const onResponse &cb);
    void onRecvData(const char *data, uint64_t size);

protected:
    virtual bool onProcessRequest(const string &cmd, uint64_t seq, const Value &obj, const string &body = "") = 0;
    virtual int onSendData(const string &data) = 0;
    virtual void onResponseSizeChanged(int size) {};
    //管理_mapResponse中的对象
    void manageResponse(int code, const string &msg, bool flushAll = false);

private:
    virtual ssize_t onRecvHeader(const char *data, size_t len) override;
    virtual void onRecvContent(const char *data, size_t len) override;

private:
    //处理收到的包
    void sendPacket(const Value &header, const string &body);
    void handlePacket(const Value &header, const string &body);
    void handleRequest(const Value &header, const string &body);
    void handleResponse(const Value &header, const string &body);

protected:
    Ticker _aliveTicker;

private:
    Value _header;
    //发送的请求的seq
    uint64_t _reqSeq = 0;
    unordered_map<uint64_t, ResponseBinder::Ptr> _mapResponse;
    SubClassType _subclass;
};


} /* namespace Proxy */
#endif /* PROXYPROTOCOL_H_ */
