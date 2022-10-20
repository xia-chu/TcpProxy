#import "WaitingHUD.h"
#import <objc/runtime.h>
#import "NSString+Valid.h"
static char hudKey;
@implementation WaitingHUD
{
    __weak IBOutlet NSLayoutConstraint *con_img_width;
    __weak IBOutlet NSLayoutConstraint *con_title_top;
}
-(id)init{
    self=[super init];
    if (self) {
        NSArray *arr=[[NSBundle mainBundle] loadNibNamed:@"WaitingHUD" owner:self options:nil];
        self=[arr firstObject];
    }
    return self;
}
- (void)startAnimation{
    [loadingView startAnimating];
    view_progress.hidden    = true;
    con_img_width.constant  = 40;
}
- (void)stopAnimation{
    [loadingView stopAnimating];
    view_progress.hidden    = false;
    con_img_width.constant  = _userImage ? 40 : 0;
}
-(void)setMessage:(NSString *)message{
    _message                = message;
    lb_title.text           = message;
    con_title_top.constant  = [message notEmpty] * 4;
}

-(void)setUserImage:(UIImage *)userImage{
    _userImage              = userImage;
    view_progress.image     = userImage;
    if(userImage){
        [self stopAnimation];
    }
}

@end

@implementation UIView (WaitingHUD)

-(void)setHud:(UIView *)hud{
    if (self.hud) {
        [self.hud removeFromSuperview];
    }
    objc_setAssociatedObject(self,&hudKey,hud,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}
-(WaitingHUD *)hud{
    return objc_getAssociatedObject(self, &hudKey);
}
-(WaitingHUD *)makeHud{
    WaitingHUD *hud=[[WaitingHUD alloc] init];
    [self addSubview:hud];
    self.hud = hud;
    CGRect rect;
    if (self.superview ) {
        rect=[self.superview convertRect:self.frame
                                  toView:[[UIApplication sharedApplication].windows lastObject]];
    }else{
        rect=self.frame;
    }
    
    hud.translatesAutoresizingMaskIntoConstraints=NO;
    [self addConstraints:[NSLayoutConstraint
                          constraintsWithVisualFormat:@"H:|[hud]|"
                          options:0
                          metrics:nil
                          views:NSDictionaryOfVariableBindings(hud)]];
    [self addConstraints:[NSLayoutConstraint
                          constraintsWithVisualFormat:[NSString stringWithFormat:@"V:|-(%.1f)-[hud]|",64-rect.origin.y]
                          options:0
                          metrics:nil
                          views:NSDictionaryOfVariableBindings(hud)]];
    
    ((UIView *)[hud.subviews firstObject]).alpha=0;
    [UIView animateWithDuration:0.25 animations:^{
        ((UIView *)[hud.subviews firstObject]).alpha=1.0;
    }];
    [[UIView topScreen] endEditing:YES];
    return hud;
}

-(void)showHUD:(NSString *)message{
    WaitingHUD *hud = [self makeHud];
    hud.message=message;
    [hud startAnimation];
}

-(void)dismissHUDWithImage:(UIImage *)img Message:(NSString *)message Delay:(float)delay{
    WaitingHUD *hud=self.hud;
    if (!hud) {
        return;
    }
    hud.userInteractionEnabled=false;
    hud.message=message;
    hud.userImage=img;
    [hud->lb_title.superview setNeedsLayout];
    [UIView animateWithDuration:0.1 animations:^{
        [hud->lb_title.superview layoutIfNeeded];
    }];
    [UIView animateWithDuration:0.25
                          delay:delay
                        options:UIViewAnimationOptionCurveEaseInOut
                     animations:^{
                         ((UIView *)[hud.subviews firstObject]).alpha=0;
                     } completion:^(BOOL finished) {
                         [hud removeFromSuperview];
                         if(self.hud == hud){
                             self.hud = nil;
                         }
                     }];
}
-(void)dismissHUDWithError:(NSString *)message{
    [self dismissHUDWithImage:[UIImage imageNamed:@"hud_error"] Message:message Delay:[UIView timeForDelay:message]];
}
-(void)dismissHUDWithInfo:(NSString *)message{
    [self dismissHUDWithImage:[UIImage imageNamed:@"hud_info"] Message:message Delay:[UIView timeForDelay:message]];
}
-(void)dismissHUDWithSuccess:(NSString *)message{
    [self dismissHUDWithImage:[UIImage imageNamed:@"hud_success"] Message:message Delay:[UIView timeForDelay:message]];
}
-(void)dismissHUD{
    [self dismissHUDWithImage:self.hud.userImage Message:self.hud.message Delay:0.25];
}


+(UIView *) topScreen{
    return [[UIApplication sharedApplication].windows firstObject];
}

+(void)showToastImage:(UIImage*)img  Message:(NSString *)msg{
    WaitingHUD *hud             = [[UIView topScreen] makeHud];
    hud.message                 = msg;
    hud.userImage               = img;
    hud.userInteractionEnabled  = false;
    [UIView animateWithDuration:0.25
                          delay:[UIView timeForDelay:msg]
                        options:UIViewAnimationOptionCurveEaseInOut
                     animations:^{
                         ((UIView *)[hud.subviews firstObject]).alpha=0;
                     } completion:^(BOOL finished) {
                         [hud removeFromSuperview];
                         if([UIView topScreen].hud == hud){
                             [UIView topScreen].hud = nil;
                         }
                     }];
}

+(void)showToastSuccess:(NSString *)msg{
    [UIView showToastImage:[UIImage imageNamed:@"hud_success"] Message:msg];
}
+(void)showToastError:(NSString *)msg{
    [UIView showToastImage:[UIImage imageNamed:@"hud_error"] Message:msg];
}

+(void)showToastInfo:(NSString *)msg{
    [UIView showToastImage:[UIImage imageNamed:@"hud_info"] Message:msg];
}
+(float)timeForDelay:(NSString *) str{
    return  MIN((CGFloat)str.length * 0.06 + 0.5, 5);
}
@end
