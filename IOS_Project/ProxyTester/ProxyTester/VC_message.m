//
//  VC_message.m
//  ProxyTester
//
//  Created by xzl on 2018/12/4.
//  Copyright © 2018 xzl. All rights reserved.
//

#import "VC_message.h"
#import "ProxySDK.h"
#import "WaitingHUD.h"
@interface VC_message ()

@end

@implementation VC_message
{
    __weak IBOutlet UITextField *_txt_message;
    __weak IBOutlet UITextField *_txt_room_id;
    __weak IBOutlet UITextField *_txt_target;
}
- (void)viewDidLoad {
    [super viewDidLoad];
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onProxyEvent:) name:kProxyEventMessage object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onProxyEvent:) name:kProxyEventJoinRoom object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onProxyEvent:) name:kProxyEventExitRoom object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onProxyEvent:) name:kProxyEventRoomMessage object:nil];
}
- (IBAction)onClick_singalMessage:(id)sender {
    [ProxySDK sendMessageTo:_txt_target.text
                   withData:[_txt_message.text  dataUsingEncoding:NSUTF8StringEncoding]
                andCallBack:^(int code, NSString *message, NSString *obj, NSData *content) {
                    if (code == 0) {
                        [UIView showToastSuccess:[NSString stringWithFormat:@"发送成功:%@,%@,%@",message,obj,
                                                 [NSString stringWithUTF8String:content.bytes ? content.bytes : ""]]];
                    }else{
                        [UIView showToastSuccess:[NSString stringWithFormat:@"发送失败:%d,%@,%@,%@",code,message,obj,
                                                 [NSString stringWithUTF8String:content.bytes ? content.bytes : ""]]];
                    }
    }];
}
- (IBAction)onClick_roomMessage:(id)sender {
    [ProxySDK broadcastMessageToRoom:_txt_room_id.text
                   withData:[_txt_message.text  dataUsingEncoding:NSUTF8StringEncoding]
                andCallBack:^(int code, NSString *message, NSString *obj, NSData *content) {
                    if (code == 0) {
                        [UIView showToastSuccess:[NSString stringWithFormat:@"发送成功:%@,%@,%@",message,obj,
                                                  [NSString stringWithUTF8String:content.bytes ? content.bytes : ""]]];
                    }else{
                        [UIView showToastSuccess:[NSString stringWithFormat:@"发送失败:%d,%@,%@,%@",code,message,obj,
                                                  [NSString stringWithUTF8String:content.bytes ? content.bytes : ""]]];
                    }
                }];
}
- (IBAction)onClick_exitRoom:(id)sender {
    [ProxySDK exitRoom:_txt_room_id.text withCallBack:^(int code, NSString *message, NSString *obj, NSData *content) {
        if(code == 0){
            [self.navigationController popViewControllerAnimated:YES];
            return ;
        }
        [UIView showToastSuccess:[NSString stringWithFormat:@"失败:%d,%@,%@,%@",code,message,obj,
                                  [NSString stringWithUTF8String:content.bytes ? content.bytes : ""]]];
    }];
}


-(void) onProxyEvent:(NSNotification *) not{
    NSDictionary *dic = not.object;
    if ([not.name isEqualToString:kProxyEventJoinRoom]) {
        [UIView showToastSuccess:[NSString stringWithFormat:@"%@ 加入房间:%@",
                                  dic[@"from"],
                                  dic[@"room_id"]]];
        
    }else if ([not.name isEqualToString:kProxyEventExitRoom]) {
        [UIView showToastSuccess:[NSString stringWithFormat:@"%@ 退出房间:%@",
                                  dic[@"from"],
                                  dic[@"room_id"]]];
    }else if ([not.name isEqualToString:kProxyEventMessage]) {
        NSData *data = dic[@"data"];
        ResponseBlock block = dic[@"response"];
        [UIView showToastSuccess:[NSString stringWithFormat:@"%@ 发给我消息:%@",
                                  dic[@"from"],
                                  [NSString stringWithUTF8String:data.bytes]]];
        block(0,@"ios response success",@"{\"ios_key\" : \" ios_val \"}",data);
    }else if ([not.name isEqualToString:kProxyEventRoomMessage]) {
        NSData *data = dic[@"data"];
        [UIView showToastSuccess:[NSString stringWithFormat:@"%@ 发给 %@ 群消息:%@",
                                  dic[@"from"],
                                  dic[@"room_id"],
                                  [NSString stringWithUTF8String:data.bytes]]];
    }
}

@end
