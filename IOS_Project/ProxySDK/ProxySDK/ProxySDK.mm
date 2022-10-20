#import "ProxySDK.h"
#import "Proxy.h"
#import "Util/onceToken.h"
#import "Util/logger.h"

using namespace toolkit;

NSString * const kProxyEventLoginResult  = @"kProxyEventLoginResult";
NSString * const kProxyEventShutdown     = @"kProxyEventShutdown";
NSString * const kProxyEventBinded       = @"kProxyEventBinded";
NSString * const kProxyEventSendSpeed    = @"kProxyEventSendSpeed";
NSString * const kProxyEventMessage      = @"kProxyEventMessage";
NSString * const kProxyEventRoomMessage  = @"kProxyEventRoomMessage";
NSString * const kProxyEventJoinRoom     = @"kProxyEventJoinRoom";
NSString * const kProxyEventExitRoom     = @"kProxyEventExitRoom";

#define ATTACH_VAL(dic) \
do{\
    [dic setObject:@(from) forKey:@"from"]; \
    [dic setObject:@(room_id) forKey:@"room_id"]; \
    [dic setObject:@(obj) forKey:@"obj"]; \
    NSData *nsdata = [[NSData alloc] initWithBytes:content length:content_len]; \
    [dic setObject:nsdata forKey:@"data"];\
}while(0)


@implementation ProxySDK

+(void) initSDK{
    static onceToken token([](){
        initProxySDK();
        setLoginCB([](void *userData,int errCode ,const char *errMsg){
            NSDictionary *dic = @{@"code":@(errCode),@"msg":[NSString stringWithUTF8String:errMsg]};
            dispatch_async(dispatch_get_main_queue(), ^{
                [[NSNotificationCenter defaultCenter] postNotificationName:kProxyEventLoginResult object:dic];
            });
        },nullptr);
        
        setShutdownCB([](void *userData,int errCode ,const char *errMsg){
            NSDictionary *dic = @{@"code":@(errCode),@"msg":[NSString stringWithUTF8String:errMsg]};
            dispatch_async(dispatch_get_main_queue(), ^{
                [[NSNotificationCenter defaultCenter] postNotificationName:kProxyEventShutdown object:dic];
            });
        },nullptr);
        
        setBindCB([](void *userData,const char *binder){
            NSDictionary *dic = @{@"binder":[NSString stringWithUTF8String:binder]};
            dispatch_async(dispatch_get_main_queue(), ^{
                [[NSNotificationCenter defaultCenter] postNotificationName:kProxyEventBinded object:dic];
            });
        },nullptr);
        
        setSendSpeedCB([](void *userData,const char *dst_uuid,uint64_t bytesPerSec){
            NSDictionary *dic = @{@"bytesPerSec":@(bytesPerSec),@"dst":[NSString stringWithUTF8String:dst_uuid]};
            dispatch_async(dispatch_get_main_queue(), ^{
                [[NSNotificationCenter defaultCenter] postNotificationName:kProxyEventSendSpeed object:dic];
            });
        },nullptr);
        
        setMessageCB([](void *userData, const char *from, const char *obj, const char *content, int content_len, void *invoker){
            ResponseBlock block = ^(int code,NSString *message,NSString *obj,NSData *content){
                ProxyResponseDoAndRelease(invoker, code, [message UTF8String], [obj UTF8String], (char *)content.bytes, (int)content.length);
            };
            
            NSMutableDictionary *dic = [[NSMutableDictionary alloc] init];
            [dic setObject:@(from) forKey:@"from"];
            [dic setObject:@(obj) forKey:@"obj"];
            NSData *nsdata = [[NSData alloc] initWithBytes:content length:content_len];
            [dic setObject:nsdata forKey:@"data"];
            [dic setObject:block forKey:@"response"];
            dispatch_async(dispatch_get_main_queue(), ^{
                [[NSNotificationCenter defaultCenter] postNotificationName:kProxyEventMessage object:dic];
            });
        }, nullptr);
        
        setJoinRoomCB([](void *userData, const char *from, const char *room_id, const char *obj, const char *content, int content_len, void *invoker){
            ProxyResponseDoAndRelease(invoker, 0, "", nullptr, "", 0);
            NSMutableDictionary *dic = [[NSMutableDictionary alloc] init];
            ATTACH_VAL(dic);
            dispatch_async(dispatch_get_main_queue(), ^{
                [[NSNotificationCenter defaultCenter] postNotificationName:kProxyEventJoinRoom object:dic];
            });
        }, nullptr);
        
        setExitRoomCB([](void *userData, const char *from, const char *room_id, const char *obj, const char *content, int content_len, void *invoker){
            ProxyResponseDoAndRelease(invoker, 0, "", nullptr, "", 0);
            NSMutableDictionary *dic = [[NSMutableDictionary alloc] init];
            ATTACH_VAL(dic);
            dispatch_async(dispatch_get_main_queue(), ^{
                [[NSNotificationCenter defaultCenter] postNotificationName:kProxyEventExitRoom object:dic];
            });
        }, nullptr);
        
        
        setRoomBroadcastCB([](void *userData, const char *from, const char *room_id, const char *obj, const char *content, int content_len, void *invoker){
            ProxyResponseDoAndRelease(invoker, 0, "", nullptr, "", 0);
            NSMutableDictionary *dic = [[NSMutableDictionary alloc] init];
            ATTACH_VAL(dic);
            dispatch_async(dispatch_get_main_queue(), ^{
                [[NSNotificationCenter defaultCenter] postNotificationName:kProxyEventRoomMessage object:dic];
            });
        }, nullptr);
        
    },[](){
        setLoginCB(nullptr,nullptr);
        setShutdownCB(nullptr,nullptr);
        setBindCB(nullptr,nullptr);
        setSendSpeedCB(nullptr,nullptr);
        setMessageCB(nullptr,nullptr);
        setJoinRoomCB(nullptr,nullptr);
        setExitRoomCB(nullptr,nullptr);
        setRoomBroadcastCB(nullptr,nullptr);
    });
}

+(void) loginWithSrvUrl:(NSString *)srv_url
                srvPort:(uint16_t) srv_port
               userName:(NSString *)user_name
               userInfo:(NSDictionary *)user_info
                 useSSL:(BOOL)use_ssl{
    [ProxySDK initSDK];
    
    clearUserInfo();
    for (NSString *key in user_info) {
        setUserInfo([key UTF8String],[user_info[key] UTF8String]);
    }
    
    login([srv_url UTF8String],srv_port,[user_name UTF8String],use_ssl);
}

+(void) logout{
    logout();
}


+(int) getStatus{
    return getStatus();
}

static ProxyResponseCB s_response = [](void *userData,
                                       int code,
                                       const char *message,
                                       const char *obj,
                                       const char *content,
                                       int content_len){
    
    NSString *ns_message = [NSString stringWithUTF8String:message];
    NSString *ns_obj = [NSString stringWithUTF8String:obj];
    NSData *ns_content =  [NSData dataWithBytes:content length:content_len];
    
    dispatch_async(dispatch_get_main_queue(), ^{
        ResponseBlock cb_ptr = CFBridgingRelease(userData);
        cb_ptr(code,ns_message,ns_obj,ns_content);
    });
    
};



+(void) sendMessageTo:(NSString*) dst_user
             withData:(NSData *) data
          andCallBack:(ResponseBlock) callback{
    sendMessage([dst_user UTF8String], (char *)data.bytes, (int)data.length , (void*)CFBridgingRetain(callback), s_response);
}


/**
 * 广播消息至房间
 * @param room_id 房间id
 * @param data 消息内容
 * @param callback 回调
 */
+(void) broadcastMessageToRoom:(NSString*) room_id
                      withData:(NSData *) data
                   andCallBack:(ResponseBlock) callback{
    broadcastRoom([room_id UTF8String], (char *)data.bytes, (int)data.length, (void*)CFBridgingRetain(callback), s_response);
}


/**
 * 加入房间
 * @param room_id 房间id
 * @param callback 回调
 */
+(void) joinRoom:(NSString*) room_id
    withCallBack:(ResponseBlock) callback{
    joinRoom([room_id UTF8String], (void*)CFBridgingRetain(callback), s_response);
}


/**
 * 退出
 * @param room_id 房间id
 * @param callback 回调
 */
+(void) exitRoom:(NSString*) room_id
    withCallBack:(ResponseBlock) callback{
    exitRoom([room_id UTF8String], (void*)CFBridgingRetain(callback), s_response);
}

@end


@implementation ProxyBinder
{
    BinderContext _contex;
}
-(instancetype) init{
    self = [super init];
    if (self) {
        _contex = createBinder();
        
        binder_setBindResultCB(_contex,[](void *userData,int errCode,const char *errMsg){
            ProxyBinder *binder = (__bridge ProxyBinder *)userData;
            NSDictionary *dic = @{@"code":@(errCode),@"msg":[NSString stringWithUTF8String:errMsg]};
            dispatch_async(dispatch_get_main_queue(), ^{
                if ([binder.delegate respondsToSelector:@selector(onBindResult:binder:)]) {
                    [binder.delegate onBindResult:dic binder:binder];
                }
            });
            
        },(__bridge void *)self);
        
        binder_setSocketErrCB(_contex,[](void *userData,int errCode,const char *errMsg){
            ProxyBinder *binder = (__bridge ProxyBinder *)userData;
            NSDictionary *dic = @{@"code":@(errCode),@"msg":[NSString stringWithUTF8String:errMsg]};
            dispatch_async(dispatch_get_main_queue(), ^{
                if ([binder.delegate respondsToSelector:@selector(onSocketErr:binder:)]) {
                    [binder.delegate onSocketErr:dic binder:binder];
                }
            });
        },(__bridge void *)self);
    }
    return self;
}
-(void) dealloc{
    binder_setBindResultCB(_contex,nullptr,nullptr);
    binder_setSocketErrCB(_contex,nullptr,nullptr);
    releaseBinder(_contex);
}

-(int) bindUser:(NSString *)user
        dstPort:(uint16_t) d_port
      localPort:(uint16_t) l_port{
    return binder_bind(_contex,
                       [user UTF8String],
                       d_port,
                       l_port);
}
-(int) bindUser:(NSString *)dst_user
        dstPort:(uint16_t)dst_port
      localPort:(uint16_t)local_port
         dstUrl:(NSString *)dst_url
        localIp:(NSString *)local_ip{
    return binder_bind2(_contex,
                        [dst_user UTF8String],
                        dst_port,
                        local_port,
                        [dst_url UTF8String],
                        [local_ip UTF8String]);
}
@end












