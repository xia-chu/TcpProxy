//
//  VC_addBinder.h
//  ProxyTester
//
//  Created by xzl on 2017/7/12.
//  Copyright © 2017年 xzl. All rights reserved.
//

#import <UIKit/UIKit.h>

typedef void(^onAddBinder)(NSDictionary *binderInfo);
@interface VC_addBinder : UIViewController<UITextFieldDelegate>
@property (nonatomic,copy) onAddBinder block;
@end
