
// VNDegen spike: the iOS 27 SDK refuses to launch apps without the UIScene life cycle, and SDL2 still creates
// its UIWindow with -initWithFrame:. Adopt scenes through Info.plist and attach every new window to the scene.
@import UIKit;
#import <objc/runtime.h>

static UIWindowScene *VNDScene(void) {
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if ([scene isKindOfClass:UIWindowScene.class]) return (UIWindowScene *)scene;
    }
    return nil;
}

@interface VNDSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation VNDSceneDelegate
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)options {
    for (UIWindow *window in UIApplication.sharedApplication.windows) {
        if (window.windowScene == nil) window.windowScene = (UIWindowScene *)scene;
    }
    NSLog(@"VNDegen scene connected");
}
@end

@implementation UIWindow (VNDScene)
+ (void)load {
    method_exchangeImplementations(class_getInstanceMethod(self, @selector(initWithFrame:)),
                                   class_getInstanceMethod(self, @selector(vnd_initWithFrame:)));
}

- (instancetype)vnd_initWithFrame:(CGRect)frame {
    UIWindow *window = [self vnd_initWithFrame:frame];
    UIWindowScene *scene = VNDScene();
    if (scene != nil && window.windowScene == nil) window.windowScene = scene;
    NSLog(@"VNDegen window %@ scene %@", window, scene);
    return window;
}
@end
