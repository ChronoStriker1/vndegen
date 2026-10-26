// Puts the engine's view inside this app's own view hierarchy, under a label the app draws on top, and runs the
// engine from a CADisplayLink, as VNDegen will. -clickAfter <seconds> -clickX <0..1> -clickY <0..1> also sends one left
// click through vnkk_mouse_button, the path VNDegen's controller input takes. -gameRoot2 and -data2 name a second game:
// -switchAfter <seconds> quits the first game through vnkk_request_quit, and once it has quit the second game starts in
// the same process, as when a VNDegen player returns to the library and picks another game.
#import <UIKit/UIKit.h>
#import <KiriKiri/vnkk.h>
#include <unistd.h>

@interface HostViewController : UIViewController
@end

@implementation HostViewController {
    CADisplayLink *_displayLink;
    UIView *_engineView;
    NSUInteger _game;
    CFTimeInterval _gameStart;
    BOOL _quitRequested;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations
{
    return UIInterfaceOrientationMaskLandscape;
}

- (BOOL)prefersStatusBarHidden
{
    return YES;
}

// The game root and data arguments of each game: -gameRoot and -data, then -gameRoot2 and -data2.
- (NSArray<NSArray<NSString *> *> *)games
{
    NSUserDefaults *arguments = NSUserDefaults.standardUserDefaults;
    NSMutableArray *games = [NSMutableArray array];
    for (NSString *suffix in @[ @"", @"2" ]) {
        NSString *gameRoot = [arguments stringForKey:[@"gameRoot" stringByAppendingString:suffix]];
        NSString *data = [arguments stringForKey:[@"data" stringByAppendingString:suffix]];
        if (gameRoot && data)
            [games addObject:@[ gameRoot, data ]];
    }
    return games;
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    if (_displayLink)
        return;

    UILabel *label = [UILabel new];
    label.text = @" test host ";
    label.textColor = UIColor.whiteColor;
    label.backgroundColor = [UIColor.systemRedColor colorWithAlphaComponent:0.8];
    [label sizeToFit];
    label.center = CGPointMake(CGRectGetMidX(self.view.bounds), CGRectGetMaxY(self.view.bounds) - 20);
    label.layer.zPosition = 1;
    [self.view addSubview:label];

    if (![self startGame:0])
        return;

    NSUserDefaults *arguments = NSUserDefaults.standardUserDefaults;
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

- (BOOL)startGame:(NSUInteger)index
{
    NSArray<NSArray<NSString *> *> *games = self.games;
    if (index >= games.count) {
        NSLog(@"KiriKiriTestHost: no game %lu", (unsigned long)index + 1);
        return NO;
    }
    NSString *log = [[NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask]
                         .firstObject URLByAppendingPathComponent:@"kirikiri.log"].path;
    VNKKLaunch launch = {games[index][0].fileSystemRepresentation, games[index][1].fileSystemRepresentation,
                         log.fileSystemRepresentation};
    NSLog(@"KiriKiriTestHost: starting game %lu in process %d", (unsigned long)index + 1, getpid());
    if (vnkk_start(&launch) != 0) {
        NSLog(@"KiriKiriTestHost: the game did not start");
        vnkk_stop();
        return NO;
    }
    _game = index;
    _gameStart = CACurrentMediaTime();
    _quitRequested = NO;

    _engineView = (__bridge UIView *)vnkk_view();
    _engineView.frame = self.view.bounds;
    _engineView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view insertSubview:_engineView atIndex:0];

    // SDL made its own window key when it showed it.
    [self.view.window makeKeyAndVisible];
    NSLog(@"KiriKiriTestHost: engine view %@ in %@", _engineView, _engineView.superview);
    return YES;
}

- (void)step
{
    double switchAfter = [NSUserDefaults.standardUserDefaults doubleForKey:@"switchAfter"];
    if (switchAfter > 0 && !_quitRequested && _game + 1 < self.games.count &&
        CACurrentMediaTime() - _gameStart > switchAfter) {
        NSLog(@"KiriKiriTestHost: quitting game %lu", (unsigned long)_game + 1);
        _quitRequested = YES;
        vnkk_request_quit();
    }
    if (!vnkk_iterate())
        return;
    [_engineView removeFromSuperview];
    _engineView = nil;
    vnkk_stop();
    NSLog(@"KiriKiriTestHost: game %lu quit", (unsigned long)_game + 1);
    if (!_quitRequested || ![self startGame:_game + 1])
        [_displayLink invalidate];
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
