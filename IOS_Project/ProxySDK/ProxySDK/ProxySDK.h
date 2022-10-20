#import <Foundation/Foundation.h>

#if defined(__cplusplus)
#define FOUNDATION_EXTERN extern "C"
#else
#define FOUNDATION_EXTERN extern
#endif

typedef enum : NSInteger{
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
}  ERROR_CODE;

typedef void(^ResponseBlock)(int code,NSString *message,NSString *obj,NSData *content);

/**
 * 登录结果事件广播,广播参数是一个NSDictionary,里面包含code和msg键
 * code 错误代码
 * msg 错误提示
 */
FOUNDATION_EXTERN NSString * const kProxyEventLoginResult;

/* 
 * 掉线事件广播,广播参数是一个NSDictionary,里面包含code和msg键
 * code 错误代码
 * msg 错误提示或对方ip
 */
FOUNDATION_EXTERN NSString * const kProxyEventShutdown;

/**
 * 设备被绑定事件广播,广播参数是一个NSDictionary,里面包含binder键
 * binder 绑定者用户名
 */
FOUNDATION_EXTERN NSString * const kProxyEventBinded;


/**
 * 设备上传至某用户网速统计事件广播,广播参数是一个NSDictionary,包含bytesPerSec和dst键
 * bytesPerSec 网速,单位B/S
 * dst 目标用户名
 */
FOUNDATION_EXTERN NSString * const kProxyEventSendSpeed;


/**
 * 收到消息广播事件
 * 广播参数是一个NSDictionary,包含 from/room_id/obj/data 键
 * from 发送者id
 * obj json对象，字符串方式展示
 * data 消息主体内容
 * response ResponseBlock类型的回调block，请执行只之
 */
FOUNDATION_EXTERN NSString * const kProxyEventMessage;

/**
 * 收到房间消息广播事件
 * 广播参数是一个NSDictionary,包含 from/room_id/obj/data 键
 * from 发送者id
 * room_id 房间id
 * obj json对象，字符串方式展示
 * data 消息主体内容
 */
FOUNDATION_EXTERN NSString * const kProxyEventRoomMessage;


/**
 * 其他人进入房间事件
 * 广播参数是一个NSDictionary,包含 from/room_id/obj/data 键
 * from 发送者id
 * room_id 房间id
 * obj json对象，字符串方式展示
 * data 消息主体内容
 */
FOUNDATION_EXTERN NSString * const kProxyEventJoinRoom;


/**
 * 其他人退出房间事件
 * 广播参数是一个NSDictionary,包含 from/room_id/obj/data 键
 * from 发送者id
 * room_id 房间id
 * obj json对象，字符串方式展示
 * data 消息主体内容
 */
FOUNDATION_EXTERN NSString * const kProxyEventExitRoom;





@interface ProxySDK : NSObject

/**
 * 初始化sdk
 */
+(void) initSDK;
/**
 * 异步登录
 * @param srv_url 服务器ip或域名
 * @param srv_port 服务器端口
 * @param user_name 登录用户名
 * @param user_info 用户描述信息,比如说用户的设备信息等
 * @param use_ssl 服务器端口是否为加密端口
 */
+(void) loginWithSrvUrl:(NSString *)srv_url
                srvPort:(uint16_t) srv_port
               userName:(NSString *)user_name
               userInfo:(NSDictionary *)user_info
                 useSSL:(BOOL)use_ssl;

/**
 * 异步登出
 */
+(void) logout;


/**
 * 获取当前登录状态
 * @return 登录状态;0:未登录 1:登录中 2:已登录
 */
+(int) getStatus;


/**
 * 发送数据给某用户
 * @param dst_user 目标用户ID
 * @param data 消息内容
 * @param callback 回调
 */
+(void) sendMessageTo:(NSString*) dst_user
             withData:(NSData *) data
          andCallBack:(ResponseBlock) callback;


/**
 * 广播消息至房间
 * @param room_id 房间id
 * @param data 消息内容
 * @param callback 回调
 */
+(void) broadcastMessageToRoom:(NSString*) room_id
                      withData:(NSData *) data
                   andCallBack:(ResponseBlock) callback;


/**
 * 加入房间
 * @param room_id 房间id
 * @param callback 回调
 */
+(void) joinRoom:(NSString*) room_id
    withCallBack:(ResponseBlock) callback;


/**
 * 退出
 * @param room_id 房间id
 * @param callback 回调
 */
+(void) exitRoom:(NSString*) room_id
    withCallBack:(ResponseBlock) callback;

@end



@class ProxyBinder;

/**
 * 绑定器事件代理
 */
@protocol ProxyBinderDelegate <NSObject>
@optional

/**
 * 绑定结果事件回调
 * @param result 绑定结果详情,包含code和msg键
 *  code 错误代码
 *  msg 错误提示
 * @param binder 绑定器
 */
-(void) onBindResult:(NSDictionary *)result binder:(ProxyBinder *)binder;

/**
 * 远程主机汇报连接TCP服务失败事件回调
 * @param err 失败详情,包含code和msg键
 *  code 错误代码
 *  msg 错误提示
 * @param binder 绑定器
 */
-(void) onSocketErr:(NSDictionary *) err binder:(ProxyBinder *) binder;
@end


/**
 * 绑定器
 */
@interface ProxyBinder : NSObject
/**
 * 事件监听者
 */
@property(nonatomic,weak) id<ProxyBinderDelegate> delegate;

/**
 * 触发绑定动作;绑定成功后,访问本机映射的TCP端口犹如访问目标主机
 * @param dst_user 目标主机
 * @param dst_port 目标主机端口号
 * @param local_port 映射至本机的端口号,如果传入0则让系统随机分配
 * @return 本机端口号,-1代表绑定本机端口失败(可能已占用,也可能没有权限使用该端口)
 */
-(int) bindUser:(NSString *)dst_user
        dstPort:(uint16_t)dst_port
      localPort:(uint16_t)local_port;

/**
 * 触发绑定动作;绑定成功后,访问本机映射的TCP端口将触发目标主机去访问dst_url:dst_port对应的主机
 * @param dst_user 目标主机
 * @param dst_port 目标主机访问的端口号
 * @param local_port 映射至本机的端口号,如果传入0则让系统随机分配
 * @param dst_url 目标主机访问的ip或域名
 * @param local_ip 本机监听ip地址,建议127.0.0.1或0.0.0.0
 * @return 本机端口号,-1代表绑定本机端口失败(可能已占用,也可能没有权限使用该端口)
 */
-(int) bindUser:(NSString *)dst_user
        dstPort:(uint16_t)dst_port
      localPort:(uint16_t)local_port
         dstUrl:(NSString *)dst_url
        localIp:(NSString *)local_ip;

@end








