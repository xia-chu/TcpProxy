//
//  ViewController.m
//  ProxyTester
//
//  Created by xzl on 2017/7/11.
//  Copyright © 2017年 xzl. All rights reserved.
//

#import "VC_login.h"
#import "NSString+Valid.h"
#import "Common.h"
#import "ProxySDK.h"
#import "WaitingHUD.h"

@interface VC_login ()

@end

@implementation VC_login
{
    __weak IBOutlet UITextField *txt_user;
    __weak IBOutlet UISwitch *sw_ssl;
    __weak IBOutlet UITextField *txt_passwd;
    __weak IBOutlet UITextField *txt_app_id;
    __weak IBOutlet UITextField *txt_server;
    
}
- (void)viewDidLoad {
    [super viewDidLoad];
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onProxyEvent:) name:kProxyEventLoginResult object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onProxyEvent:) name:kProxyEventShutdown object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onProxyEvent:) name:kProxyEventBinded object:nil];
    
    if([[NSUserDefaults standardUserDefaults] integerForKey:@"txt_port"]){
        txt_user.text = [[NSUserDefaults standardUserDefaults] stringForKey:@"txt_user"];
        sw_ssl.on = [[NSUserDefaults standardUserDefaults] boolForKey:@"sw_ssl"];
    }
}
-(void) onProxyEvent:(NSNotification *) not{
    if ([not.name isEqualToString:kProxyEventLoginResult]) {
        NSDictionary *dic = not.object;
        if ([dic[@"code"] intValue] == 0) {
            [self.view dismissHUD];
            [self performSegueWithIdentifier:@"seg_showMenu" sender:self];
            
            [[NSUserDefaults standardUserDefaults] setObject:txt_user.text forKey:@"txt_user"];
            [[NSUserDefaults standardUserDefaults] setBool:sw_ssl.on forKey:@"sw_ssl"];
            [[NSUserDefaults standardUserDefaults] synchronize];
        }else{
            [self.view dismissHUDWithInfo:dic[@"msg"]];
        }
    }else if ([not.name isEqualToString:kProxyEventShutdown]) {
        NSDictionary *dic = not.object;
        [UIView showToastInfo:[NSString stringWithFormat:@"已经掉线:%@,%@",dic[@"code"],dic[@"msg"]]];
        [self.navigationController popToRootViewControllerAnimated:YES];
    }else if ([not.name isEqualToString:kProxyEventBinded]) {
        NSDictionary *dic = not.object;
        [UIView showToastSuccess:[NSString stringWithFormat:@"被%@绑定",dic[@"binder"]]];
    }
}
-(BOOL) textFieldShouldReturn:(UITextField *)textField{
    if (![textField.text notEmpty]) {
        [Common shakeAnimationForView:textField];
        return false;
    }
    if (textField == txt_user) {
        [self click_login:nil];
    }
    return true;
}
- (IBAction)click_login:(id)sender {
    if(![txt_user.text notEmpty]){
        [Common shakeAnimationForView:txt_user];
        return;
    }
    
    if(![txt_passwd.text notEmpty]){
        [Common shakeAnimationForView:txt_passwd];
        return;
    }
    
    if(![txt_app_id.text notEmpty]){
        [Common shakeAnimationForView:txt_app_id];
        return;
    }
    
    if(![txt_server.text notEmpty]){
        [Common shakeAnimationForView:txt_server];
        return;
    }
    
    [self.view showHUD:@"登录中..."];
    
    @try {
        [ProxySDK loginWithSrvUrl:[txt_server.text componentsSeparatedByString:@":"][0]
                          srvPort:[[txt_server.text componentsSeparatedByString:@":"][1] intValue]
                         userName:txt_user.text
                         userInfo:@{@"token": txt_passwd.text,
                                    @"appId": txt_app_id.text}
                           useSSL:sw_ssl.on];
    } @catch (NSException *exception) {
        [self.view dismissHUDWithError:exception.description];
    }
}


- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}


@end
