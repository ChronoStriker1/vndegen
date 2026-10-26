// Puts the engine's view inside this app's own view hierarchy, under a label the app draws on top, and runs the
// engine from a CADisplayLink, as VNDegen will. -clickAfter <seconds> -clickX <0..1> -clickY <0..1> also sends one left
// click through vnkk_mouse_button, the path VNDegen's controller input takes.
#import <UIKit/UIKit.h>
#import <KiriKiri/vnkk.h>

@interface HostViewController : UIViewController
@end

@implementation HostViewController {
    CADisplayLink *_displayLink;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations
{
    return UIInterfaceOrientationMaskLandscape;
}

- (BOOL)prefersStatusBarHidden
{
    return YES;
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    if (_displayLink)
        return;
    NSUserDefaults *arguments = NSUserDefaults.standardUserDefaults;
    NSString *gameRoot = [arguments stringForKey:@"gameRoot"];
    NSString *data = [arguments stringForKey:@"data"];
    NSString *log = [[NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask]
                         .firstObject URLByAppendingPathComponent:@"kirikiri.log"].path;
    VNKKLaunch launch = {gameRoot.fileSystemRepresentation, data.fileSystemRepresentation, log.fileSystemRepresentation};
    if (!gameRoot || !data || vnkk_start(&launch) != 0) {
        NSLog(@"KiriKiriTestHost: the game did not start");
        vnkk_stop();
        return;
    }

    UIView *engineView = (__bridge UIView *)vnkk_view();
    engineView.frame = self.view.bounds;
    engineView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:engineView];

    UILabel *label = [UILabel new];
    label.text = @" test host ";
    label.textColor = UIColor.whiteColor;
    label.backgroundColor = [UIColor.systemRedColor colorWithAlphaComponent:0.8];
    [label sizeToFit];
    label.center = CGPointMake(CGRectGetMidX(self.view.bounds), CGRectGetMaxY(self.view.bounds) - 20);
    [self.view addSubview:label];

    // SDL made its own window key when it showed it.
    [self.view.window makeKeyAndVisible];
    NSLog(@"KiriKiriTestHost: engine view %@ in %@", engineView, engineView.superview);

    double clickAfter = [arguments doubleForKey:@"clickAfter"];
    if (clickAfter > 0) {
        float x = [arguments floatForKey:@"clickX"], y = [arguments floatForKey:@"clickY"];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(clickAfter * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            NSLog(@"KiriKiriTestHost: clicking at %.3f, %.3f", x, y);
            vnkk_mouse_move(x, y);
            vnkk_mouse_button(1, true, x, y);
            vnkk_mouse_button(1, false, x, y);
        });
    }

    _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(step)];
    [_displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
}

- (void)step
{
    if (vnkk_iterate()) {
        [_displayLink invalidate];
        vnkk_stop();
        NSLog(@"KiriKiriTestHost: the game quit");
    }
}

@end

@interface SceneDelegate : UIResponder <UIWindowSceneDelegate>
@property (nonatomic) UIWindow *window;
@end

@implementation SceneDelegate

- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)options
{
    self.window = [[UIWindow alloc] initWithWindowScene:(UIWindowScene *)scene];
    self.window.rootViewController = [HostViewController new];
    [self.window makeKeyAndVisible];
}

@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation AppDelegate
@end

int main(int argc, char *argv[])
{
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass(AppDelegate.class));
    }
}
