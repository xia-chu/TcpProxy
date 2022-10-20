#ifndef PROXY_PROXY_H_
#define PROXY_PROXY_H_

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
    
#if defined(_WIN32)
#if defined(TcpProxy_shared_EXPORTS)
#define API_EXPORT __declspec(dllexport)
#else
#define API_EXPORT __declspec(dllimport)
#endif //defined(TcpProxy_shared_EXPORTS)
    
#define PROXYCALL __cdecl
#else
#define API_EXPORT
#define PROXYCALL
#endif
 
    
/**
 * ProxySDK,一个TCP远程映射工具SDK
 * ----------------------------
 * v1.10 2017/12/22
 * 1、数据包阈值设置为32K~512K
 * 2、添加配置相关接口
 *
 * v1.00 2017/11/19
 * 1、初始提交
 *
 */

/////////////////////////环境初始化////////////////////////////////

/**
 * 初始化库,在调用其他API前必须先执行该API
 * 建议进入main函数最先执行
 * 程序生命周期只能调用一次。
 */
API_EXPORT void  PROXYCALL initProxySDK();

/**
 * 设置全局参数配置
 * @param key 配置名
 * @param value 配置值
 */
API_EXPORT void PROXYCALL setGlobalOption(const char *key, const char *value);

/**
 * 获取全局参数配置
 * @param key 配置名
 * @param value 配置值存放地址
 * @return NULL失败，否则为value指针
 */
API_EXPORT char* PROXYCALL getGlobalOption(const char *key, char *value);

/**
 * 以ini格式导出全局配置
 * @param out 存放内存地址
 * @param out_size 存放内存大小指针，内部会赋值为所需内存大小
 * @return 如果返回-1代表失败,否则成功
 */
API_EXPORT int PROXYCALL dumpOptionsIni(char *out,int *out_size);

/**
 * 以ini格式加载全局配置
 * @param ini_str ini配置字符串
 * @return 如果返回-1代表失败，否则成功
 */
API_EXPORT int PROXYCALL loadOptionsIni(const char *ini_str);

/////////////////////////用户登录////////////////////////////////

/**
 * 设置登录时的用户其他描述信息,比如说用户的设备信息
 * @param key 描述名
 * @param value 描述内容
 */
API_EXPORT void PROXYCALL setUserInfo(const char *key, const char *value);

/**
 * 清空登录描述信息
 */
API_EXPORT void PROXYCALL clearUserInfo();

/**
 * 异步登录
 * @param srv_url 服务器ip或域名
 * @param srv_port 服务器端口
 * @param user_name 登录用户名
 * @param use_ssl 是否使用ssl
 */
API_EXPORT void PROXYCALL login(const char *srv_url, unsigned short srv_port, const char *user_name,int use_ssl);

/**
 * 异步登出
 */
API_EXPORT void PROXYCALL logout();

/**
 * 获取当前登录状态
 * @return 登录状态;0:未登录 1:登录中 2:已登录
 */
API_EXPORT int PROXYCALL getStatus();

/**
 * 登录结果回调
 * @param userData 用户数据指针
 * @param errCode 0:成功 1:超时 2:认证失败 3:不支持 4:未找到 100:其他
 * @param errMsg 错误提示
 */
typedef void(PROXYCALL *ProxyLoginResultCB)(void *userData, int errCode, const char *errMsg);

/**
 * 掉线事件
 * @param userData 用户数据指针
 * @param errCode 1:收到EOF,网络链接关断 2:超时 3:连接被拒绝 4:dns解析错误 5:其他 100:挤占掉线,此时errMsg为对方ip
 * @param errMsg 错误提示或对方ip
 */
typedef void(PROXYCALL *ProxyShutdownCB)(void *userData, int errCode, const char *errMsg);

/**
 * 被其他用户绑定事件
 * @param userData 用户数据指针
 * @param binder 对端登录用户名
 */
typedef void(PROXYCALL *ProxyBindedCB)(void *userData, const char *binder);

/**
 * 发送网速统计事件,通过该接口可以评估网络状况从而实现诸如动态调整视频码率等需求
 * @param userData 用户数据指针
 * @param dst_user 目标用户
 * @param bytesPerSec 发送网速,单位B/S
 */
typedef void(PROXYCALL *ProxySendSpeedCB)(void *userData, const char *dst_user, uint64_t bytesPerSec);

/**
 * 发送网速统计事件,通过该接口可以评估网络状况从而实现诸如动态调整视频码率等需求
 * @param userData 用户数据指针
 * @param code 错误代码，0代表成功
 * @param message 错误提示
 * @param obj json对象
 * @param content 回复消息主体
 * @param content_len 回复消息主体长度，为0时内部通过strlen函数获取content的长度
 */
typedef void(PROXYCALL *ProxyResponseCB)(void *userData, int code, const char *message, const char *obj,const char *content,int content_len);

/**
 * 执行回复
 * @param invoker 回复对象指针
 * @param code 错误代码，0代表成功
 * @param message 错误提示
 * @param obj json对象
 * @param content 回复消息主体
 * @param content_len 回复消息主体长度，为0时内部通过strlen函数获取content的长度
 */
API_EXPORT void PROXYCALL ProxyResponseDoAndRelease(void *invoker, int code, const char *message, const char *obj,const char *content,int content_len);

/**
 * 收到消息回调
 * @param userData 用户数据指针
 * @param from 发送者
 * @param obj json对象
 * @param content 消息主体
 * @param content_len 消息主体长度
 * @param invoker 回复函数, 请用ProxyResponseDoAndRelease执行之
 */
typedef void(PROXYCALL *ProxyMessageCB)(void *userData, const char *from, const char *obj, const char *content, int content_len, void *invoker);

/**
 * 收到房间相关事件
 * @param userData 用户数据指针
 * @param from 发送者
 * @param room_id 房间id
 * @param obj json对象
 * @param content 消息主体
 * @param content_len 消息主体长度
 * @param invoker 回复函数, 请用ProxyResponseDoAndRelease执行之
 */
typedef void(PROXYCALL *ProxyRoomMessageCB)(void *userData, const char *from, const char *room_id, const char *obj, const char *content, int content_len, void *invoker);

/**
 * 设置登录结果回调函数
 * @param cb 回调函数
 * @param userData 用户数据指针
 * @see ProxyLoginResultCB
 */
API_EXPORT void PROXYCALL setLoginCB(ProxyLoginResultCB cb, void *userData);

/**
 * 设置掉线回调函数
 * @param cb 回调函数
 * @param userData 用户数据指针
 * @see ProxyShutdownCB
 */
API_EXPORT void PROXYCALL setShutdownCB(ProxyShutdownCB cb, void *userData);

/**
 * 设置被绑定的回调函数
 * @param cb 回调函数
 * @param userData 用户数据指针
 * @see ProxyBindedCB
 */
API_EXPORT void PROXYCALL setBindCB(ProxyBindedCB cb, void *userData);

/**
 * 设置统计发送网速回调函数
 * @param cb 回调函数
 * @param userData 用户数据指针
 * @see ProxySendSpeedCB
 */
API_EXPORT void PROXYCALL setSendSpeedCB(ProxySendSpeedCB cb, void *userData);

/**
 * 设置接收到消息回调函数
 * @param cb 回调函数
 * @param userData 回调函数用户指针
 */
API_EXPORT void PROXYCALL setMessageCB(ProxyMessageCB cb, void *userData);

/**
 * 设置他人加入群组事件回调函数
 * @param cb 回调函数
 * @param userData 回调函数用户指针
 */
API_EXPORT void PROXYCALL setJoinRoomCB(ProxyRoomMessageCB cb, void *userData);

/**
 * 设置他人退出群组事件回调函数
 * @param cb 回调函数
 * @param userData 回调函数用户指针
 */
API_EXPORT void PROXYCALL setExitRoomCB(ProxyRoomMessageCB cb, void *userData);

/**
 * 设置接受到群消息事件回调函数
 * @param cb 回调函数
 * @param userData 回调函数用户指针
 */
API_EXPORT void PROXYCALL setRoomBroadcastCB(ProxyRoomMessageCB cb, void *userData);

/////////////////////////房间相关API已经message相关api////////////////////////////////

/**
 * 发送即时消息给对方
 * @param to 目标用户id
 * @param content 消息主体内容
 * @param content_len 消息主体长度，为0时内部通过strlen函数获取content的长度
 * @param user_data 用户数据指针
 * @param callback  对方回复回调函数,可以为null
 */
API_EXPORT void PROXYCALL sendMessage(const char *to,const char *content,int content_len,void *user_data, ProxyResponseCB callback);

/**
 * 加入房间
 * @param room_id 目标用户id
 * @param user_data 用户数据指针
 * @param callback  对方回复回调函数,可以为null
 */
API_EXPORT void PROXYCALL joinRoom(const char *room_id,void *user_data, ProxyResponseCB callback);

/**
 * 退出房间
 * @param room_id 目标用户id
 * @param user_data 用户数据指针
 * @param callback  对方回复回调函数,可以为null
 */
API_EXPORT void PROXYCALL exitRoom(const char *room_id,void *user_data, ProxyResponseCB callback);

/**
 * 发送房间消息广播
 * @param room_id 房间id
 * @param content 消息主体内容
 * @param content_len 消息主体长度，为0时内部通过strlen函数获取content的长度
 * @param user_data 用户数据指针
 * @param callback  回调函数,可以为null
 */
API_EXPORT void PROXYCALL broadcastRoom(const char *room_id,const char *content,int content_len,void *user_data, ProxyResponseCB callback);

/////////////////////////绑定远程TCP服务////////////////////////////////

//绑定上下文
typedef void* BinderContext;

/**
 * 绑定结果回调函数定义
 * @param userData 用户数据指针
 * @param errCode 0:成功 1:超时 2:认证失败 3:不支持 4:未找到 100:其他
 * @param errMsg 错误提示
 */
typedef void(PROXYCALL *ProxyBindResultCB)(void *userData, int errCode, const char *errMsg);

/**
 * 远程主机连接TCP服务失败回调函数,通过该接口可以实现诸如探测远程主机TCP服务是否开启等需求
 * @param userData 用户数据指针
 * @param errCode 1:收到EOF,网络链接关断 2:超时 3:连接被拒绝 4:dns解析错误 5:其他
 * @param errMsg 错误提示
 */
typedef void(PROXYCALL *ProxySocketErrCB)(void *userData, int errCode, const char *errMsg);

/**
 * 创建绑定上下文
 * @see releaseBinder
 * @return 上下文句柄
 */
API_EXPORT BinderContext PROXYCALL createBinder();

/**
 * 设置绑定结果回调函数
 * @param ctx 绑定上下文
 * @param cb 回调函数
 * @param userData 用户数据指针
 * @see ProxyBindResultCB
 */
API_EXPORT void PROXYCALL binder_setBindResultCB(BinderContext ctx, ProxyBindResultCB cb, void *userData);

/**
 * 设置远程主机汇报连接TCP服务失败回调函数
 * @param ctx 绑定上下文
 * @param cb 回调函数
 * @param userData 用户数据指针
 * @see ProxySocketErrCB
 * @see createBinder
 * 返回值:无
 */
API_EXPORT void PROXYCALL binder_setSocketErrCB(BinderContext ctx, ProxySocketErrCB cb, void *userData);

/**
 * 触发绑定动作;绑定成功后,访问本机映射的TCP端口犹如访问目标主机
 * @param ctx 绑定上下文
 * @param dst_user 目标主机
 * @param dst_port 目标主机TCP服务端口号
 * @param self_port 映射至本机的端口号,如果传入0则让系统随机分配
 * @return 本机端口号,-1代表绑定本机端口失败(可能已占用,也可能没有权限使用该端口,详情请看SDK日志打印)
 */
API_EXPORT int PROXYCALL binder_bind(BinderContext ctx, const char *dst_user,
                                     unsigned short dst_port, unsigned short self_port);

/**
 * 触发绑定动作;绑定成功后,访问本机映射的TCP端口将触发目标主机去访问dst_url:dst_port对应的主机
 * @param ctx 绑定上下文
 * @param dst_user 目标主机
 * @param dst_port 目标主机访问的TCP服务端口号
 * @param self_port 映射至本机的端口号,如果传入0则让系统随机分配
 * @param dst_url 目标主机访问的ip或域名
 * @param self_ip 本机监听ip地址,建议127.0.0.1或0.0.0.0
 * @return 本机端口号,-1代表绑定本机端口失败(可能已占用,也可能没有权限使用该端口,详情请看SDK日志打印)
 */
API_EXPORT int PROXYCALL binder_bind2(BinderContext ctx, const char *dst_user, unsigned short dst_port,
                                      unsigned short self_port, const char *dst_url, const char *self_ip);

/**
 * 释放绑定上下文
 * @param ctx 上下文句柄
 * @see createBinder
 */
API_EXPORT void PROXYCALL releaseBinder(BinderContext ctx);


/////////////////////////日志////////////////////////////////

/**
 * 日志级别
 */
typedef enum {
    LogTrace = 0, LogDebug, LogInfo, LogWarn, LogError, LogFatal,
} LogType;

/**
 * 日志输出回调函数
 * @param strLog 日志字符串
 * @param iLogLen 字符串长度
 */
typedef void(PROXYCALL *onLogOut)(const char *strLog, int iLogLen);

/**
 * 设置Log输出回调
 * @param cb 日志输出回调函数
 * @see onLogOut
 */
API_EXPORT void PROXYCALL log_setOnLogOut(onLogOut cb);

/**
 * 设在日志显示级别
 * @param level 日志级别
 * @see LogType
 */
API_EXPORT void PROXYCALL log_setLevel(LogType level);

/**
 * 打印日志
 * @param level 日志级别
 * @param file 文件名称
 * @param function 函数名称
 * @param line 所在行数
 * @param fmt 格式化字符串,用法跟printf一致
 */
API_EXPORT void PROXYCALL log_printf(LogType level, const char* file, const char* function, int line, const char *fmt, ...);
    
//各种级别打印日志
#define log_trace(fmt,...) log_printf(LogTrace,__FILE__,__FUNCTION__,__LINE__,fmt,##__VA_ARGS__)
#define log_debug(fmt,...) log_printf(LogDebug,__FILE__,__FUNCTION__,__LINE__,fmt,##__VA_ARGS__)
#define log_info(fmt,...) log_printf(LogInfo,__FILE__,__FUNCTION__,__LINE__,fmt,##__VA_ARGS__)
#define log_warn(fmt,...) log_printf(LogWarn,__FILE__,__FUNCTION__,__LINE__,fmt,##__VA_ARGS__)
#define log_error(fmt,...) log_printf(LogError,__FILE__,__FUNCTION__,__LINE__,fmt,##__VA_ARGS__)
#define log_fatal(fmt,...) log_printf(LogFatal,__FILE__,__FUNCTION__,__LINE__,fmt,##__VA_ARGS__)
    
    
#ifdef __cplusplus
}
#endif

#endif /* PROXY_PROXY_H_ */
