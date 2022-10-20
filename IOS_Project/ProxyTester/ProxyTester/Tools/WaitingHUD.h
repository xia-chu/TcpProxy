#import <UIKit/UIKit.h>

@interface WaitingHUD : UIView
{
@public
    __weak IBOutlet UILabel                     *lb_title;
    __weak IBOutlet UIImageView                 *view_progress;
    __weak IBOutlet UIActivityIndicatorView     *loadingView;
}
@property(nonatomic,copy)   NSString            *message;
@property(nonatomic,retain) UIImage             *userImage;
- (void) startAnimation;
- (void) stopAnimation;
@end

@interface UIView (WaitingHUD)
@property(nonatomic,retain) WaitingHUD          *hud;

- (void) showHUD:(NSString *)message;

- (void) dismissHUDWithImage:(UIImage *)img Message:(NSString *)message Delay:(float)delay;
- (void) dismissHUDWithSuccess:(NSString *)message;
- (void) dismissHUDWithInfo:(NSString *)message;
- (void) dismissHUDWithError:(NSString *)message;
- (void) dismissHUD;

+ (UIView *) topScreen;
+ (void) showToastImage:(UIImage*)img  Message:(NSString *)message;
+ (void) showToastSuccess:(NSString *)message;
+ (void) showToastInfo:(NSString *)message;
+ (void) showToastError:(NSString *)message;
@end
