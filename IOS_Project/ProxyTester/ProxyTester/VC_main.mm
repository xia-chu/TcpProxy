//
//  VC_main.m
//  ProxyTester
//
//  Created by xzl on 2017/7/11.
//  Copyright © 2017年 xzl. All rights reserved.
//

#import "VC_main.h"
#import "ProxySDK.h"
#import "Proxy.h"

@interface VC_main ()

@end

@implementation VC_main
{
    __weak IBOutlet UILabel *lb_speed;
    __weak IBOutlet UITextView *txt_log;
    NSMutableString *str_log;
    
}

- (void)viewDidLoad {
    [super viewDidLoad];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onProxyEvent:) name:kProxyEventSendSpeed object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onLog:) name:@"onLogOut" object:nil];
    
    str_log = [NSMutableString string];
    log_setOnLogOut([](const char *strLog,int iLogLen){
        NSString *str = [NSString stringWithUTF8String:strLog];
        dispatch_async(dispatch_get_main_queue(), ^{
            [[NSNotificationCenter defaultCenter] postNotificationName:@"onLogOut" object:str];
        });
    });
    
}
-(void) onLog:(NSNotification *) notice{
    [str_log appendString:(NSString *)notice.object];
    txt_log.text = str_log;
}
-(void) dealloc
{
    log_setOnLogOut(nullptr);
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

-(void) onProxyEvent:(NSNotification *) notice
{
     if ([notice.name isEqualToString:kProxyEventSendSpeed]) {
         NSDictionary *dic = notice.object;
         lb_speed.text = [NSString stringWithFormat:@"发送速度[%@]:%.2f KB/s",dic[@"dst"],[dic[@"bytesPerSec"] longLongValue] / 1024.0];
    }
}

@end
