/*
 * Config.h
 *
 *  Created on: 2017年6月12日
 *      Author: xzl
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include "Util/mini.h"
using namespace toolkit;

namespace Config {
namespace Broadcast{
///////////////////////////////客户端上的事件///////////////////////////////
//被绑定事件
extern const char kBroadcastOnBind[];
#define BroadcastOnBindArgs const string &binder

//发送网速回调
extern const char kBroadcastSendSpeed[];
#define BroadcastSendSpeedArgs const string &dst_uuid,uint64_t &speed

//收到数据请求
extern const char kBroadcastOnMessage[];
#define BroadcastOnMessageArgs const string &from ,const Value &obj,const string &data ,const onResponse &invoker

//其他人进入房间广播
extern const char kBroadcastOnJoinRoom[];
#define BroadcastOnJoinRoomArgs const string &from ,const string &room_id,const Value &obj,const string &data ,const onResponse &invoker

//其他人退出房间广播
extern const char kBroadcastOnExitRoom[];
#define BroadcastOnExitRoomArgs const string &from ,const string &room_id,const Value &obj,const string &data ,const onResponse &invoker

//房间信息广播
extern const char kBroadcastOnRoomBroadcast[];
#define BroadcastOnRoomBroadcastArgs const string &from ,const string &room_id,const Value &obj,const string &data ,const onResponse &invoker


///////////////////////////////服务器上的事件///////////////////////////////
//用户登录鉴权
extern const char kBroadcastOnUserLoginAuth[];
#define BroadcastOnUserLoginAuthArgs const std::shared_ptr<TcpSession> &session,\
                                     const string &appId,\
                                     const string &uuid,\
                                     const string &token,\
                                     const Value &obj,\
                                     const ProxySession::AuthInvoker &invoker

//用户登录或登出事件
extern const char kBroadcastOnUserLog[];
#define BroadcastOnUserLogArgs  const std::shared_ptr<TcpSession> &session, \
                                bool &login,\
                                const string &appId,\
                                const string &uuid

//会话跳转事件,请分配可用的服务器地址
extern const char kBroadcastSessionJump[];
#define BroadcastSessionJumpArgs string &retSrvUrl,\
                                 uint16_t &retSrvPort,\
                                 const uint16_t &local_port


//用户进入房间广播
extern const char kBroadcastOnUserJoinRoom[];
#define BroadcastOnUserJoinRoomArgs const std::shared_ptr<TcpSession> &session, \
                                    const string &appId,\
                                    const string &uuid,\
                                    const string &roomid,\
                                    const ProxySession::AuthInvoker &invoker

//用户退出房间广播
extern const char kBroadcastOnUserExitRoom[];
#define BroadcastOnUserExitRoomArgs const std::shared_ptr<TcpSession> &session,\
                                    const string &appId,\
                                    const string &uuid,\
                                    const unordered_set<string> &room_ids

//普通消息
extern const char kBroadcastOnUserMessage[];
#define BroadcastOnUserMessageArgs const std::shared_ptr<TcpSession> &session,\
                                   const string &appId,\
                                   const string &from,\
                                   const string &to,\
                                   const string &cmd,\
                                   const Value& obj,\
                                   const string& body,\
                                   const ProxySession::ResponseInvoker &invoker

//房间添加或删除了
extern const char kBroadcastOnRoomCountChanged[];
#define BroadcastOnRoomCountChangedArgs const string &appId,const string &room_id,const bool &add_or_del


//消息记录
extern const char kBroadcastOnMessageHistory[];
#define BroadcastOnMessageHistoryArgs   const string &appId, \
                                        const string &from, \
                                        const string &to, \
                                        const Value &obj, \
                                        const string &body, \
                                        const bool &success, \
                                        const bool &roomMessage


}//Broadcast

namespace Session{
//会话超时时间,单位秒，默认45秒
extern const char kTimeoutSec[];
//心跳时间间隔，单位秒，默认15秒
extern const char kBeatInterval[];
}//Session

namespace Proxy{

//加载配置文件，如果配置文件不存在，那么会导出默认配置并生成配置文件
//加载配置文件成功后会触发kBroadcastUpdateConfig广播
//如果指定的文件名(ini_path)为空，那么会加载默认配置文件
//默认配置文件名为 /path/to/your/exe.ini
//加载配置文件成功后返回true，否则返回false
bool loadIniConfig(const char *ini_path = nullptr);

//ProxyChannel超时时间，单位秒，默认30秒，不要随意修改
extern const char kChnTimeoutSec[];
//回复超时时间，单位秒。默认15秒，不要随意修改
extern const char kResTimeoutSec[];
//包大小下限，单位KB,默认32KB
extern const char kMinPktSize[];
//包大小上限，单位KB,默认512KB
extern const char kMaxPktSize[];
//低网速情况延时优化,默认低于4KB/s时启用优化,单位KB/s
extern const char kOptimizeDelaySpeed[];
//强制刷新缓存时间，单位秒，默认0.5秒
extern const char kBufferFlushTime[];
//最多允许未ack的数据有10秒
extern const char kMaxBufferSec[];
//允许未确认的buffer最低因为256KB
extern const char kMinNoAckBufferSize[];
}//Proxy



} /* namespace Config */

#endif /* CONFIG_H_ */
