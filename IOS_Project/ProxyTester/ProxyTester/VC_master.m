//
//  VC_master.m
//  ProxyTester
//
//  Created by xzl on 2017/7/11.
//  Copyright © 2017年 xzl. All rights reserved.
//

#import "VC_master.h"
#import "VC_addBinder.h"
#import "ProxySDK.h"
#import "WaitingHUD.h"

@interface VC_master ()

@end

@implementation VC_master
{
    NSMutableArray *binderArr;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.navigationItem.hidesBackButton = YES;
    binderArr = [NSMutableArray array];
}


- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return [binderArr count];
}


- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
//    @{@"binder":binder,
//      @"dstUser":txt_dstUser.text,
//      @"dstPort":@([txt_dstPort.text intValue]),
//      @"localPort":@(localPort)};
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"Cell" forIndexPath:indexPath];
    if (!cell) {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle  reuseIdentifier:@"Cell"];
    }
    NSDictionary *dic = binderArr[indexPath.row];
    cell.textLabel.text = dic[@"dstUser"];
    cell.detailTextLabel.text = [NSString stringWithFormat:@"目标端口：%@    本地端口：%@",dic[@"dstPort"],dic[@"localPort"]];
    return cell;
}

- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath {
    return YES;
}



// Override to support editing the table view.
- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath {
    if (editingStyle == UITableViewCellEditingStyleDelete) {
        [binderArr removeObjectAtIndex:indexPath.row];
        [tableView deleteRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationFade];
    }
}



- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    if ([segue.identifier isEqualToString:@"seg_addBinder"]) {
        VC_addBinder *vc = segue.destinationViewController;
        vc.block = ^(NSDictionary *dic) {
            ProxyBinder *binder = dic[@"binder"];
            binder.delegate = (id<ProxyBinderDelegate>)self;
            [binderArr addObject:dic];
            [self.tableView insertRowsAtIndexPaths:@[[NSIndexPath indexPathForRow:(NSInteger)[binderArr count] - 1 inSection:0]]
                                  withRowAnimation:UITableViewRowAnimationLeft];
        };
    }
}

-(CGFloat) tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath{
    return 60;
}

-(void) onSocketErr:(NSDictionary *) err binder:(ProxyBinder *) binder{
    
}


@end
