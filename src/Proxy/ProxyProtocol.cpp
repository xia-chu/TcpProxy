/*
 * ProxyProtocol.cpp
 *
 *  Created on: 2017年6月14日
 *      Author: xzl
 */


#include "Util/onceToken.h"
#include "Network/sockutil.h"
#include "ProxyProtocol.h"
using namespace toolkit;

#define BODY_LEN "body_len"

namespace Proxy {

ProxyProtocol::ProxyProtocol(SubClassType type){
    _subclass = type;
    _reqSeq = ((uint64_t)this) % 10000;
}

ProxyProtocol::~ProxyProtocol() {
    manageResponse(CODE_OTHER,"~ProxyProtocol",true);
}

ssize_t ProxyProtocol::onRecvHeader(const char *data, size_t len) {
    stringstream ss(string(data,len - 4));
    ss >> _header;
    auto body_len = _header[BODY_LEN].asUInt64();
    if(body_len == 0){
        handlePacket(_header,"");
    }
    return body_len;
}

void ProxyProtocol::onRecvContent(const char *data, size_t len) {
    handlePacket(_header,string(data,len));
}

void ProxyProtocol::onRecvData(const char *data,uint64_t size) {
    TimeTicker();

    static float sec = mINI::Instance()[Config::Proxy::kResTimeoutSec];
    if(_aliveTicker.elapsedTime() > sec * 1000){
        if(HttpRequestSplitter::reset()){
            WarnL << "丢弃过期残余包数据!";
        }
    }
    _aliveTicker.resetTime();
    input(data,size);
}

void ProxyProtocol::manageResponse(int code, const string &msg,bool flushAll) {
    TimeTicker();
    bool changed = false;
    //管理回调，超时就执行并移除
    List<ResponseBinder::Ptr> call_back_list;

    for (auto it = _mapResponse.begin(); it != _mapResponse.end();) {
        if (it->second->expired() || flushAll) {
            call_back_list.emplace_back(std::move(it->second));
            it = _mapResponse.erase(it);
            changed = true;
            continue;
        }
        ++it;
    }

    call_back_list.for_each([&](ResponseBinder::Ptr &cb) {
        (*cb)(code, msg, Json::objectValue, "");
    });

    if (changed) {
        onResponseSizeChanged(_mapResponse.size());
    }
}

/*
 *
 * onRecv ----> handlePacket --------> handleRequest
 * 							|
 * 							|-------> handleResponse
 */
void ProxyProtocol::handlePacket(const Value &header, const string &body) {
    TimeTicker();
    auto type = header["type"].asString();
    typedef void (ProxyProtocol::*onHandlePacket)(const Value &header,const string &body);
    static unordered_map<string,onHandlePacket> map_type_fun;
    static onceToken token([&](){
        map_type_fun.emplace("request",&ProxyProtocol::handleRequest);
        map_type_fun.emplace("response",&ProxyProtocol::handleResponse);
    },nullptr);

    auto it = map_type_fun.find(type);
    if(it == map_type_fun.end()){
        //不会有其他类型，这里断开连接
        auto strErr = StrPrinter << "ProxyProtocol::handlePacket 不支持的命令类型:" << type << endl;
        throw std::invalid_argument(strErr);
    }
    auto fun = it->second;
    (this->*fun)(header,body);
}

/*
 *
 *  handleRequest --------> handleRequest_login
 * 					 |
 * 					 |----> handleRequest_transfer
 */
void ProxyProtocol::handleRequest(const Value &header, const string &body) {
    TimeTicker();
    /*
     * 请求类型的头：
     * {
     *   "type":"request",		// 请求类型
     *	 "seq":0,  				// 请求序号
     *	 "cmd":"transfer",		// 命令名称
     *	 "obj":{}				// 请求对象
     * }
     */
    auto seq 	= header["seq"].asUInt64();
    auto cmd 	= header["cmd"].asString();
    auto &obj	= header["obj"];
    if(!onProcessRequest(cmd,seq,obj,body)){
        throw std::runtime_error(string("onProcessRequest failed:") + cmd);
    }
}

//收到回复信令；找到回复信令匹配的回调并执行
void ProxyProtocol::handleResponse(const Value &header, const string &body) {
    TimeTicker();
    /*
     * 回复类型的头：
     * {
     * 	"type":"response",      // 回复类型
     * 	"seq": 0,			    // 回复序号（对应请求序号）
     * 	"code":0,				// 回复代码（0:成功，其他：错误）
     * 	"msg":"success",        // 回复文本（一些错误提示）
     * 	"obj":{}					// 回复对象
     * }
     */
    auto seq 	= header["seq"].asUInt64();
    auto code 	= header["code"].asInt();
    auto msg	= header["msg"].asString();
    auto &obj	= header["obj"];
    auto it 	= _mapResponse.find(seq);
    if(it == _mapResponse.end()){
        WarnL << this << " " << "未找到匹配的回调:" << seq << " " << _subclass ;
        return;
    }
    auto func = std::move(it->second);
    _mapResponse.erase(it);
    (*func)(code, msg, obj, body);
    onResponseSizeChanged(_mapResponse.size());
}

void ProxyProtocol::sendRequest(const string &cmd, const Value &obj, const string &body,const onResponse &cb) {
    TimeTicker();
    /*
     * 请求类型的头：
     * {
     *   "type":"request",		// 请求类型
     *	 "seq":0,  				// 请求序号
     *	 "cmd":"transfer",		// 命令名称
     *	 "obj":{}				// 请求对象
     * }
     */
    Value header(objectValue);
    header["type"] = "request";
    header["seq"] = (UInt64)_reqSeq;
    header["cmd"] = cmd;
    header["obj"] = obj;
    if(cb){
        _mapResponse[_reqSeq] = std::make_shared<ResponseBinder>(cb);
        onResponseSizeChanged(_mapResponse.size());
    }
    sendPacket(header,body);
    _reqSeq++;
}

void ProxyProtocol::sendResponse(uint64_t seq, int code, const string &msg,const Value &obj, const string &body) {
    TimeTicker();
    /*
     * 回复类型的头：
     * {
     * 	"type":"response",      // 回复类型
     * 	"seq": 0,			    // 回复序号（对应请求序号）
     * 	"code":0,				// 回复代码（0:成功，其他：错误）
     * 	"msg":"success",        // 回复文本（一些错误提示）
     * 	"obj":{}					// 回复对象
     * }
     */
    Value header(objectValue);
    header["type"] = "response";
    header["seq"] = (UInt64)seq;
    header["code"] = code;
    header["msg"] = msg;
    header["obj"] = obj;
    //DebugL << seq;
    sendPacket(header,body);
}

void ProxyProtocol::sendPacket(const Value &header, const string &body) {
    TimeTicker();
    const_cast<Value&>(header)[BODY_LEN] = (UInt64)body.length();
    FastWriter writer;
    onSendData(writer.write(header) + "\r\n\r\n" + body);
}


} /* namespace Proxy */
