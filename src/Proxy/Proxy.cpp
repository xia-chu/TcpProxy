/*
 * Proxy.cpp
 *
 *  Created on: 2017年6月21日
 *      Author: xzl
 */

#include <string.h>
#include <stdarg.h>
#include "Proxy.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"
#include "Network/Socket.h"
#include "ProxySession.h"
#include "ProxyTerminal.h"
#include "ProxyBinderSession.h"
#include "Config.h"

using namespace std;
using namespace toolkit;
using namespace Proxy;

//登录单例
ProxyTerminal::Ptr s_terminal;
Value s_userInfo(objectValue);

//操作系统类型
#if defined(_WIN32)
    //#warning windows
    static string s_targetos = "windows";
#elif defined(__linux__)
    #if defined(ANDROID)
        #warning android
        static string s_targetos = "android";
    #else
        #warning linux
        static string s_targetos = "linux";
    #endif
#elif defined(__APPLE__)
    #if defined(OS_IPHONE)
        #warning ios
        static string s_targetos = "ios";
    #else
        #warning macos
        static string s_targetos = "macos";
    #endif
#else
    #warning other system
    static string s_targetos = "other";
#endif // defined(_WIN32)


//cpu类型
#if defined(_X86_) ||  defined(__i386__) || defined(__x86_64__)
    #if defined(__amd64__) || defined(_X64_)
        #warning x64
        static string s_cpu = "x64";
    #else
        //#warning x86
        static string s_cpu = "x86";
    #endif
#elif defined(__arm__) || defined(__arm64__)
    #if defined(__ARM64__) || defined(__arm64__)
        #warning arm64
        static string s_cpu = "arm64";
    #else
        #warning arm
        static string s_cpu = "arm";
    #endif
#elif defined(_MIPS_)
     #warning mips
    static string s_cpu = "mips";
#else
    #warning other cpu
    static string s_cpu = "other";
#endif

#ifdef __cplusplus
extern "C" {
#endif


API_EXPORT void PROXYCALL initProxySDK() {
    static onceToken s_token([]() {
        s_userInfo["user"] = Value(objectValue);
        s_userInfo["sdk"]["os"] = s_targetos;
        s_userInfo["sdk"]["cpu"] = s_cpu;
        s_userInfo["sdk"]["ver"] = "1.0";
        s_userInfo["sdk"]["time"] = __DATE__ " " __TIME__;
    }, nullptr);
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    s_terminal.reset(new ProxyTerminal);
}

API_EXPORT void PROXYCALL setGlobalOption(const char *key, const char *value){
    if(!key || !value){
        return;
    }
    if(mINI::Instance().find(key) == mINI::Instance().end()){
        WarnL << "option " << key << " not exist!";
        return;
    }
    mINI::Instance()[key] = value;
    InfoL << "set option: " << key << " = " << value;
}

API_EXPORT char * PROXYCALL getGlobalOption(const char *key, char *value){
    if(!key || !value){
        return nullptr;
    }
    return strcpy(value,mINI::Instance()[key].data());
}

API_EXPORT int PROXYCALL dumpOptionsIni(char *out,int *out_size){
    auto dump= mINI::Instance().dump("","");
    if(!out || !out_size){
        return -1;
    }
    int size = dump.size() + 1;
    size = size < *out_size ? size : *out_size;
    *out_size = dump.size() + 1;
    memcpy(out,dump.data(),size);
    return 0;
}

API_EXPORT int PROXYCALL loadOptionsIni(const char *ini_str){
    if(!ini_str){
        return -1;
    }
    mINI::Instance().parse(ini_str);
    return 0;
}

API_EXPORT void PROXYCALL setUserInfo(const char *key,const char *value){
    string keyStr = key;
    string valStr = value;
    EventPoller::Instance().sync([keyStr,valStr](){
        s_userInfo["user"][keyStr] = valStr;
    });
}

API_EXPORT void PROXYCALL clearUserInfo(){
    EventPoller::Instance().sync([](){
        s_userInfo["user"].clear();
    });
}

API_EXPORT void PROXYCALL login(const char *srv_url,unsigned short srv_port,const char *user_name,int use_ssl){
    string srvStr = srv_url;
    string userStr = user_name;
    EventPoller::Instance().async([srvStr,userStr,srv_port,use_ssl](){
        s_terminal->login(srvStr,srv_port,userStr, s_userInfo,use_ssl);
    });
}

API_EXPORT void PROXYCALL logout(){
    EventPoller::Instance().async([](){
        s_terminal->logout();
    });
}

API_EXPORT int PROXYCALL getStatus(){
    return s_terminal->status();
}

API_EXPORT void PROXYCALL sendMessage(const char *to_char,const char *content_char,int content_len,void *user_data, ProxyResponseCB callback){
    if(content_len <= 0){
        content_len = strlen(content_char);
    }
    string to = to_char;
    string content = string(content_char, content_len);
    EventPoller::Instance().async([to, content, user_data, callback](){
        s_terminal->message(to, objectValue, content, [user_data,callback](int code,const string &msg,const Value &obj,const string &body){
            if(callback){
                callback(user_data,code,msg.data(),obj.toStyledString().data(),body.data(),body.length());
            }
        });
    });
}

API_EXPORT void PROXYCALL joinRoom(const char *room_id_char,void *user_data, ProxyResponseCB callback){
    string room_id = room_id_char;
    EventPoller::Instance().async([room_id, user_data, callback](){
        s_terminal->joinRoom(room_id,[user_data,callback](int code,const string &msg,const Value &obj,const string &body){
            if(callback){
                callback(user_data,code,msg.data(),obj.toStyledString().data(),body.data(),body.length());
            }
        });
    });
}

API_EXPORT void PROXYCALL exitRoom(const char *room_id_char,void *user_data, ProxyResponseCB callback){
    string room_id = room_id_char;
    EventPoller::Instance().async([room_id, user_data, callback](){
        s_terminal->exitRoom(room_id,[user_data,callback](int code,const string &msg,const Value &obj,const string &body){
            if(callback){
                callback(user_data,code,msg.data(),obj.toStyledString().data(),body.data(),body.length());
            }
        });
    });
}

API_EXPORT void PROXYCALL broadcastRoom(const char *room_id_char,const char *content_char,int content_len,void *user_data, ProxyResponseCB callback){
    if(content_len <= 0){
        content_len = strlen(content_char);
    }
    string room_id = room_id_char;
    string content = string(content_char, content_len);
    EventPoller::Instance().async([room_id, content, user_data, callback](){
        s_terminal->broadcastRoom(room_id, content, [user_data,callback](int code,const string &msg,const Value &obj,const string &body){
            if(callback){
                callback(user_data,code,msg.data(),obj.toStyledString().data(),body.data(),body.length());
            }
        });
    });
}

API_EXPORT void PROXYCALL setLoginCB(ProxyLoginResultCB cb,void *userData){
    EventPoller::Instance().sync([&](){
        if(cb){
            s_terminal->setOnLogin([cb,userData](int code,const string &msg){
                cb(userData,code,msg.data());
            });
        }else{
            s_terminal->setOnLogin(nullptr);
        }
    });
}

API_EXPORT void PROXYCALL setBindCB(ProxyBindedCB cb,void *userData){
    EventPoller::Instance().sync([&](){
        static void *tag;
        if(cb){
            NoticeCenter::Instance().addListener(&tag,Config::Broadcast::kBroadcastOnBind,[userData,cb](BroadcastOnBindArgs){
                cb(userData,binder.data());
            });
        }else{
            NoticeCenter::Instance().delListener(&tag);
        }
    });
}

API_EXPORT void PROXYCALL setSendSpeedCB(ProxySendSpeedCB cb,void *userData){
    EventPoller::Instance().sync([&]() {
        static void *tag;
        if (cb) {
            NoticeCenter::Instance().addListener(&tag, Config::Broadcast::kBroadcastSendSpeed, [userData, cb](BroadcastSendSpeedArgs) {
                cb(userData, dst_uuid.data(), speed);
            });
        } else {
            NoticeCenter::Instance().delListener(&tag);
        }
    });
}

API_EXPORT void PROXYCALL setShutdownCB(ProxyShutdownCB cb,void *userData){
    EventPoller::Instance().sync([&](){
        if(cb){
            s_terminal->setOnShutdown([cb,userData](const SockException &ex){
                cb(userData,ex.getErrCode(),ex.what());
            });
        }else{
            s_terminal->setOnShutdown(nullptr);
        }
    });
}

API_EXPORT void PROXYCALL ProxyResponseDoAndRelease(void *userData, int code, const char *message, const char *obj,const char *content,int content_len){
    onResponse *invoker = (onResponse *) userData;
    try {
        Value val(objectValue);
        if (obj && obj[0]) {
            stringstream ss(obj);
            ss >> val;
        }
        if (content_len <= 0) {
            content_len = strlen(content ? content : "");
        }
        (*invoker)(code, message, val, string(content ? content : "", content_len));
    } catch (std::exception &ex) {
        WarnL << ex.what();
    }
    delete invoker;
}

API_EXPORT void PROXYCALL setMessageCB(ProxyMessageCB cb, void *userData){
    EventPoller::Instance().sync([&]() {
        static void *tag;
        if (cb) {
            NoticeCenter::Instance().addListener(&tag,Config::Broadcast::kBroadcastOnMessage,[userData, cb](BroadcastOnMessageArgs){
                cb(userData, from.data(), obj.toStyledString().data(), data.data(), data.size(), (void *)new onResponse(invoker));
            });
        } else {
            NoticeCenter::Instance().delListener(&tag);
        }
    });
}

API_EXPORT void PROXYCALL setJoinRoomCB(ProxyRoomMessageCB cb, void *userData){
    EventPoller::Instance().sync([&]() {
        static void *tag;
        if (cb) {
            NoticeCenter::Instance().addListener(&tag,Config::Broadcast::kBroadcastOnJoinRoom,[userData, cb](BroadcastOnJoinRoomArgs){
                cb(userData, from.data(), room_id.data(), obj.toStyledString().data(), data.data(), data.size(), (void *)new onResponse(invoker));
            });
        } else {
            NoticeCenter::Instance().delListener(&tag);
        }
    });
}

API_EXPORT void PROXYCALL setExitRoomCB(ProxyRoomMessageCB cb, void *userData){
    EventPoller::Instance().sync([&]() {
        static void *tag;
        if (cb) {
            NoticeCenter::Instance().addListener(&tag,Config::Broadcast::kBroadcastOnExitRoom,[userData, cb](BroadcastOnExitRoomArgs){
                cb(userData, from.data(), room_id.data(), obj.toStyledString().data(), data.data(), data.size(), (void *)new onResponse(invoker));
            });
        } else {
            NoticeCenter::Instance().delListener(&tag);
        }
    });
}

API_EXPORT void PROXYCALL setRoomBroadcastCB(ProxyRoomMessageCB cb, void *userData){
    EventPoller::Instance().sync([&]() {
        static void *tag;
        if (cb) {
            NoticeCenter::Instance().addListener(&tag,Config::Broadcast::kBroadcastOnRoomBroadcast,[userData, cb](BroadcastOnRoomBroadcastArgs){
                cb(userData, from.data(), room_id.data(), obj.toStyledString().data(), data.data(), data.size(), (void *)new onResponse(invoker));
            });
        } else {
            NoticeCenter::Instance().delListener(&tag);
        }
    });
}

API_EXPORT BinderContext PROXYCALL createBinder(){
    ProxyBinder::Ptr *ret = new ProxyBinder::Ptr(new ProxyBinder(s_terminal));
    return ret;
}

API_EXPORT void PROXYCALL binder_setBindResultCB(BinderContext ctx,ProxyBindResultCB cb,void *userData){
    EventPoller::Instance().sync([&](){
        ProxyBinder::Ptr &binder = *((ProxyBinder::Ptr *)ctx);
        if(cb){
            binder->setBindResultCB([cb,userData](int code,const string &msg,const Value &obj,const string &body){
                cb(userData,code,msg.data());
            });
        }else{
            binder->setBindResultCB(nullptr);
        }
    });
}

API_EXPORT void PROXYCALL binder_setSocketErrCB(BinderContext ctx,ProxySocketErrCB cb,void *userData){
    EventPoller::Instance().sync([&](){
        ProxyBinder::Ptr &binder = *((ProxyBinder::Ptr *)ctx);
        if(cb){
            binder->setSocketErrCB([cb,userData](const SockException &ex){
                cb(userData,ex.getErrCode(),ex.what());
            });
        }else{
            binder->setSocketErrCB(nullptr);
        }
    });
}

API_EXPORT int PROXYCALL binder_bind(BinderContext ctx,const char *dst_user,unsigned short dst_port,unsigned short self_port){
    return binder_bind2(ctx,dst_user,dst_port,self_port,"127.0.0.1","0.0.0.0");
}
API_EXPORT int PROXYCALL binder_bind2(BinderContext ctx,const char *dst_user,unsigned short dst_port,unsigned short self_port, const char *dst_ip,const char *self_ip){
    int ret = -1;
    EventPoller::Instance().sync([&]() {
        ProxyBinder::Ptr &binder = *((ProxyBinder::Ptr *) ctx);
        try {
            ret = binder->start(self_port, self_ip);
            binder->bind(dst_user, dst_port, dst_ip);
        } catch (std::exception &ex) {
            WarnL << ex.what();
            ret = -1;
        }
    });
    return ret;
}

API_EXPORT void PROXYCALL releaseBinder(BinderContext ctx){
    EventPoller::Instance().async([ctx](){
        delete (ProxyBinder::Ptr *)ctx;
    });
}

API_EXPORT void PROXYCALL log_printf(LogType level, const char *file, const char *function, int line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LoggerWrapper::printLogV(Logger::Instance(), level, file, function, line, fmt, args);
    va_end(args);
}

API_EXPORT void PROXYCALL log_setLevel(LogType level){
    Logger::Instance().setLevel((LogLevel)level);
}

class LogoutChannel: public LogChannel {
public:
    LogoutChannel(const string &name, onLogOut cb, LogLevel level = LDebug) :LogChannel(name, level){
        _cb = cb;
    }

    virtual ~LogoutChannel(){}

    void write(const Logger &logger,const LogContextPtr & stream) override {
        if (_level > stream->_level) {
            return;
        }
        stringstream strStream;
        format(logger, strStream, stream, false, false);
        auto strTmp = strStream.str();
        if (_cb) {
            _cb(strTmp.data(), strTmp.size());
        }
    }

private:
    onLogOut _cb = nullptr;
};

API_EXPORT void PROXYCALL log_setOnLogOut(onLogOut cb){
    std::shared_ptr<LogoutChannel> chn(new LogoutChannel("LogoutChannel",cb,LTrace));
    Logger::Instance().add(chn);
}

#ifdef __cplusplus
}
#endif







