//
//  VC_addBinder.m
//  ProxyTester
//
//  Created by xzl on 2017/7/12.
//  Copyright © 2017年 xzl. All rights reserved.
//

#import "VC_addBinder.h"
#import "Common.h"
#import "NSString+Valid.h"
#import "ProxySDK.h"
#import "WaitingHUD.h"

@interface VC_addBinder ()

@end

@implementation VC_addBinder
{
    __weak IBOutlet UITextField *txt_dstUser;
    __weak IBOutlet UITextField *txt_dstPort;
    __weak IBOutlet UITextField *txt_localPort;
    __weak IBOutlet UIView *view_back;
    
    NSDictionary *binderInfo;
}
- (void)viewDidLoad {
    [super viewDidLoad];
    UITapGestureRecognizer *rec = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(onTap:)];
    [self.view addGestureRecognizer:rec];
    
    txt_dstUser.text = [[NSUserDefaults standardUserDefaults] stringForKey:@"dstUser"];
    txt_dstPort.text = [[NSUserDefaults standardUserDefaults] stringForKey:@"dstPort"];
    txt_localPort.text = [[NSUserDefaults standardUserDefaults] stringForKey:@"localPort"];
}
-(void)onTap:(UITapGestureRecognizer *)rec{
    CGPoint pt = [rec locationInView:view_back];
    //NSLog(@"%f %f",pt.x,pt.y);
    if (pt.x < 0 || pt.y <0 || pt.x > view_back.frame.size.width || pt.y > view_back.frame.size.height) {
        [self dismissViewControllerAnimated:YES completion:nil];
    }
}

-(BOOL) textFieldShouldReturn:(UITextField *)textField{
    if (![textField.text notEmpty]) {
        [Common shakeAnimationForView:textField];
        return false;
    }
    if (textField == txt_dstUser) {
        [txt_dstPort becomeFirstResponder];
    }else if (textField == txt_dstPort){
        [txt_localPort becomeFirstResponder];
    }else{
        [self click_bind:nil];
    }
    return false;
}
- (IBAction)click_bind:(id)sender {
    if (![txt_dstUser.text notEmpty]) {
        [Common shakeAnimationForView:txt_dstUser];
        return;
    }
    if (![txt_dstPort.text notEmpty]) {
        [Common shakeAnimationForView:txt_dstPort];
        return;
    }
    if (![txt_localPort.text notEmpty]) {
        [Common shakeAnimationForView:txt_localPort];
        return;
    }
    
    [self.view showHUD:@"绑定中..."];
    binderInfo = nil;
    ProxyBinder *binder = [[ProxyBinder alloc] init];
    binder.delegate = (id<ProxyBinderDelegate>)self;
    int localPort = [binder bindUser:txt_dstUser.text
                             dstPort:[txt_dstPort.text intValue]
                           localPort:[txt_localPort.text intValue]];
    if(localPort == -1){
        [self.view dismissHUDWithInfo:@"绑定本地端口失败！"];
        return;
    }
    binderInfo = @{@"binder":binder,
                   @"dstUser":txt_dstUser.text,
                   @"dstPort":@([txt_dstPort.text intValue]),
                   @"localPort":@(localPort)};
    
}
-(void) onBindResult:(NSDictionary *) result binder:(ProxyBinder *) binder{
    if ([result[@"code"] intValue] != 0) {
        [self.view dismissHUDWithInfo:[NSString stringWithFormat:@"绑定失败:%@",result[@"msg"]]];
        return;
    }
    [self.view dismissHUDWithSuccess:@"绑定成功！"];
    [self dismissViewControllerAnimated:YES completion:nil];
    
    [[NSUserDefaults standardUserDefaults] setObject:binderInfo[@"dstUser"] forKey:@"dstUser"];
    [[NSUserDefaults standardUserDefaults] setObject:binderInfo[@"dstPort"] forKey:@"dstPort"];
    [[NSUserDefaults standardUserDefaults] setObject:binderInfo[@"localPort"] forKey:@"localPort"];
    [[NSUserDefaults standardUserDefaults] synchronize];
    
    if (_block) {
        _block(binderInfo);
    }
}

@end
