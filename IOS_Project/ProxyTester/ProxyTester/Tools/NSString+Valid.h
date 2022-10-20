
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
@interface NSString (Valid)
-(NSString *)fileFullPath;
-(BOOL)isChinese;
-(NSInteger)find:(NSString *)substr;
+(NSString *) fromInt:(NSInteger) val;
+(NSString *) fromFloat:(float) val;
+(NSString *) fromDouble:(double) val;
+(NSString *)getDataSizeString:(int) nSize;
+(NSString *)getDocumentPath;
+(NSString *)gen_uuid;
- (CGSize) sizeForFont:(UIFont*)font
     constrainedToSize:(CGSize)constraint
         lineBreakMode:(NSLineBreakMode)lineBreakMode;
-(BOOL)notEmpty;
-(NSString *)md5String;


- (NSComparisonResult) compareAsNum: (NSString *) other;

-(NSString *)encodedString;

- (NSString *)encodeToPercentEscapeString;

- (NSString *)decodeFromPercentEscapeString;

@end
