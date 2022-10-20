/*
 * Config.cpp
 *
 *  Created on: 2017年6月12日
 *      Author: xzl
 */

#include "Config.h"
#include "Util/logger.h"
#include "Util/onceToken.h"

namespace Config {

namespace Broadcast{
//被绑定事件
const char kBroadcastOnBind[] = "kBroadcastOnBind";
//发送网速回调
const char kBroadcastSendSpeed[] = "kBroadcastSendSpeed";
//用户登录或登出事件
const char kBroadcastOnUserLog[] = "kBroadcastOnUserLog";

//会话跳转事件,请分配可用的服务器地址
const char kBroadcastSessionJump[] = "kBroadcastSessionJump";

//收到数据请求
const char kBroadcastOnMessage[] = "kBroadcastOnMessage";

//其他人进入房间广播
const char kBroadcastOnJoinRoom[] = "kBroadcastOnJoinRoom";

//其他人退出房间广播
const char kBroadcastOnExitRoom[] = "kBroadcastOnExitRoom";

//房间信息广播
const char kBroadcastOnRoomBroadcast[] = "kBroadcastOnRoomBroadcast";

//用户登录鉴权
const char kBroadcastOnUserLoginAuth[] = "kBroadcastOnUserLoginAuth";

//用户进入房间广播
const char kBroadcastOnUserJoinRoom[] = "kBroadcastOnUserJoinRoom";

//用户退出房间广播
const char kBroadcastOnUserExitRoom[] = "kBroadcastOnUserExitRoom";

//用户消息
const char kBroadcastOnUserMessage[] = "kBroadcastOnUserMessage";

const char kBroadcastOnRoomCountChanged[] = "kBroadcastOnRoomCountChanged";

const char kBroadcastOnMessageHistory[] = "kBroadcastOnMessageHistory";

}//Broadcast

namespace Session{
#define SESSION_FIELD "session."
//会话超时时间,单位秒，默认45秒
const char kTimeoutSec[] = SESSION_FIELD"timeoutSec";
//心跳时间间隔，单位秒，默认15秒
const char kBeatInterval[] = SESSION_FIELD"beatInterval";

static onceToken token([](){
    mINI::Instance()[kTimeoutSec] = 45;
    mINI::Instance()[kBeatInterval] = 15;
},nullptr);
}//Session

namespace Proxy {
bool loadIniConfig(const char *ini_path) {
    std::string ini;
    if (ini_path) {
        ini = ini_path;
    } else {
        ini = exePath() + ".ini";
    }
    try {
        mINI::Instance().parseFile(ini);
        return true;
    } catch (std::exception &ex) {
        mINI::Instance().dumpFile(ini);
        return false;
    }
}

#define PROXY_FIELD "proxy."
const char kChnTimeoutSec[] = PROXY_FIELD"chnTimeoutSec";
const char kResTimeoutSec[] = PROXY_FIELD"resTimeoutSec";

const char kMinPktSize[] = PROXY_FIELD"minPktSize";
const char kMaxPktSize[] = PROXY_FIELD"maxPktSize";
const char kOptimizeDelaySpeed[] = PROXY_FIELD"optimizeDelaySpeed";
const char kBufferFlushTime[] = PROXY_FIELD"bufferFlushTime";
const char kMaxBufferSec[] = PROXY_FIELD"kMaxBufferSec";
const char kMinNoAckBufferSize[] = PROXY_FIELD"kMinNoAckBufferSize";

static onceToken token([](){
    //30秒足够长了 不会误判的
    mINI::Instance()[kChnTimeoutSec] = 30;
    //15秒的最大回复超时时间，网络太差才会这么久才回复的
    mINI::Instance()[kResTimeoutSec] = 15;
    mINI::Instance()[kMinPktSize] = 32;
    mINI::Instance()[kMaxPktSize] = 512;
    mINI::Instance()[kOptimizeDelaySpeed] = 4;
    mINI::Instance()[kBufferFlushTime] = 0.5;
    mINI::Instance()[kMaxBufferSec] = 10;
    mINI::Instance()[kMinNoAckBufferSize] = 256;
},nullptr);

}//Proxy


} /* namespace Config */
