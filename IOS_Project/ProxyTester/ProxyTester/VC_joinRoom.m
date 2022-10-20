//
//  VC_joinRoom.m
//  ProxyTester
//
//  Created by xzl on 2018/12/3.
//  Copyright © 2018 xzl. All rights reserved.
//

#import "VC_joinRoom.h"
#import "ProxySDK.h"
#import "WaitingHUD.h"
@interface VC_joinRoom ()

@end

@implementation VC_joinRoom
{
    __weak IBOutlet UITextField *_txt_room;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    _txt_room.delegate =  (id<UITextFieldDelegate>)self;
    // Do any additional setup after loading the view.
}



- (BOOL)textFieldShouldReturn:(UITextField *)textField{
    [self onClick_join: nil];
    return true;
}

- (IBAction)onClick_join:(id)sender {
    [self.view showHUD:@"加入房间..."];
    [ProxySDK joinRoom:_txt_room.text withCallBack:^(int code, NSString *message, NSString *obj, NSData *content) {
        if(code != 0){
            [self.view dismissHUDWithInfo:[NSString stringWithFormat:@"加入房间失败:%@",message]];
            return ;
        }
        [self.view dismissHUD];
        [self performSegueWithIdentifier:@"seg_joinSuccess" sender:self];
    }];
}

/*
#pragma mark - Navigation

// In a storyboard-based application, you will often want to do a little preparation before navigation
- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    // Get the new view controller using [segue destinationViewController].
    // Pass the selected object to the new view controller.
}
*/

@end
