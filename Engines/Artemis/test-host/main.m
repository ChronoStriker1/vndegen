// A throwaway iOS app that runs one Artemis game on art3m1s-core the way VNDegen will: native Metal into a
// CAMetalLayer, a CADisplayLink frame loop, and touches sent to the game as a mouse. Launch it with
// -game <root.pfs or other base archive>. The engine log is Documents/artemis.log, and saves go to Documents/save.
// Sounds and fullscreen movies are reported finished at once, since this host plays no media.
#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#include "art3m1s_api.h"

typedef struct PfsArchiveHandle PfsArchiveHandle;
PfsArchiveHandle *pfs_open_with_encoding(const char *path, const char *encoding);
int pfs_file_size(PfsArchiveHandle *archive, const char *path);
int pfs_read(PfsArchiveHandle *archive, const char *path, uint64_t offset, uint8_t *buf, uint32_t size);
void pfs_close(PfsArchiveHandle *archive);

static FILE *logFile;

static void logLine(NSString *line)
{
    fprintf(logFile, "%s\n", line.UTF8String);
    fflush(logFile);
}

@interface MetalView : UIView
@end

@implementation MetalView
+ (Class)layerClass
{
    return CAMetalLayer.class;
}
@end

@interface HostViewController : UIViewController
@end

@implementation HostViewController {
    const Art3m1sApiV1 *_api;
    HostEvents *_events;
    HostResources *_resources;
    CoreRuntime *_runtime;
    CADisplayLink *_displayLink;
    CFTimeInterval _last;
    double _carry;
    uint32_t _stageWidth, _stageHeight;
    uint64_t _frames, _presented;
}

- (void)loadView
{
    self.view = [MetalView new];
    self.view.backgroundColor = UIColor.blackColor;
    self.view.multipleTouchEnabled = NO;
}

- (BOOL)prefersStatusBarHidden
{
    return YES;
}

- (NSData *)readEntry:(NSString *)entry fromArchive:(NSString *)archive
{
    PfsArchiveHandle *pfs = pfs_open_with_encoding(archive.fileSystemRepresentation, "utf-8");
    if (!pfs)
        return nil;
    int size = pfs_file_size(pfs, entry.UTF8String);
    NSMutableData *data = size > 0 ? [NSMutableData dataWithLength:size] : nil;
    if (data && pfs_read(pfs, entry.UTF8String, 0, data.mutableBytes, (uint32_t)size) != size)
        data = nil;
    pfs_close(pfs);
    return data;
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    if (_runtime)
        return;
    NSString *documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    logFile = fopen([documents stringByAppendingPathComponent:@"artemis.log"].fileSystemRepresentation, "w");
    NSString *game = [NSUserDefaults.standardUserDefaults stringForKey:@"game"];
    size_t size = 0;
    _api = art3m1s_get_api_v1(&size);
    logLine([NSString stringWithFormat:@"api size %zu expected %zu game %@", size, sizeof(Art3m1sApiV1), game]);
    if (!_api || size != sizeof(Art3m1sApiV1) || !game)
        return;

    _events = _api->host_events_create();
    _api->host_events_enable(_events, 1);
    _api->set_debug(1);
    _resources = _api->resources_create();
    NSString *saves = [documents stringByAppendingPathComponent:@"save"];
    [NSFileManager.defaultManager createDirectoryAtPath:saves withIntermediateDirectories:YES attributes:nil error:nil];
    int mounted = _api->resources_mount_pfs(_resources, game.fileSystemRepresentation, "utf-8");
    _api->resources_set_save_dir(_resources, saves.fileSystemRepresentation);
    NSData *ini = [self readEntry:@"system.ini" fromArchive:game];
    logLine([NSString stringWithFormat:@"mount %d system.ini %lu bytes", mounted, (unsigned long)ini.length]);

    _runtime = _api->runtime_create(1280, 720, 3);
    if (!_runtime || !ini) {
        [self drainEvents];
        return;
    }
    _api->runtime_set_resources(_runtime, _resources);
    int32_t loaded = _api->runtime_load_project_bytes(_runtime, ini.bytes, ini.length, "WINDOWS");
    _stageWidth = _api->runtime_stage_width(_runtime);
    _stageHeight = _api->runtime_stage_height(_runtime);
    CAMetalLayer *layer = (CAMetalLayer *)self.view.layer;
    layer.device = MTLCreateSystemDefaultDevice();
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.contentsGravity = kCAGravityResizeAspect;
    layer.drawableSize = CGSizeMake(_stageWidth, _stageHeight);
    int32_t surface = _api->runtime_set_external_surface(_runtime, 4, (__bridge void *)layer, _stageWidth, _stageHeight);
    logLine([NSString stringWithFormat:@"load %d stage %ux%u surface %d", loaded, _stageWidth, _stageHeight, surface]);
    [self drainEvents];

    _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];
    [_displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
}

- (void)tick:(CADisplayLink *)link
{
    double elapsed = _last ? (link.timestamp - _last) * 1000.0 : 16.0;
    _last = link.timestamp;
    elapsed = MIN(elapsed, 100.0) + _carry;
    uint32_t delta = (uint32_t)elapsed;
    _carry = elapsed - delta;
    if (_api->runtime_advance_and_present(_runtime, delta) == 1)
        _presented++;
    _frames++;
    [self drainEvents];
    if (_frames % 600 == 0)
        logLine([NSString stringWithFormat:@"frames %llu presented %llu", _frames, _presented]);
}

- (void)drainEvents
{
    for (;;) {
        size_t next = _api->host_events_next(_events);
        if (!next)
            return;
        NSMutableData *buffer = [NSMutableData dataWithLength:next];
        uint32_t count = 0;
        size_t written = _api->poll_events(_events, buffer.mutableBytes, next, &count);
        if (!written || !count)
            return;
        const uint8_t *bytes = buffer.bytes;
        size_t offset = 0;
        for (uint32_t i = 0; i < count && offset + sizeof(ArtHostEventHeaderV1) <= written; i++) {
            ArtHostEventHeaderV1 header;
            memcpy(&header, bytes + offset, sizeof header);
            offset += sizeof header;
            if (offset + header.payload_size > written)
                return;
            NSString *payload = [[NSString alloc] initWithBytes:bytes + offset length:header.payload_size
                                                      encoding:NSUTF8StringEncoding] ?: @"";
            offset += header.payload_size;
            if (header.kind == 1) {
                logLine([NSString stringWithFormat:@"[%c] %@", (char)header.aux, payload]);
                continue;
            }
            logLine([NSString stringWithFormat:@"[EV%u] %@", header.kind, payload]);
            [self handleCommand:payload];
        }
    }
}

- (void)handleCommand:(NSString *)payload
{
    NSDictionary *event = [NSJSONSerialization JSONObjectWithData:[payload dataUsingEncoding:NSUTF8StringEncoding]
                                                          options:0 error:nil];
    NSString *kind = event[@"kind"];
    NSDictionary *body = event[@"payload"];
    if (![body isKindOfClass:NSDictionary.class] || [body[@"loop"] boolValue])
        return;
    id identifier = body[@"id"];
    const char *cid = [identifier isKindOfClass:NSString.class] ? [identifier UTF8String] : NULL;
    if ([kind isEqualToString:@"audio_se_play"] || [kind isEqualToString:@"audio_voice_play"])
        _api->runtime_notify_sound_finished(_runtime, cid);
    else if ([kind isEqualToString:@"video_play"] && !cid)
        _api->runtime_notify_video_finished(_runtime, NULL);
}

- (CGPoint)stagePoint:(UITouch *)touch
{
    CGSize bounds = self.view.bounds.size;
    CGFloat scale = MIN(bounds.width / _stageWidth, bounds.height / _stageHeight);
    CGFloat left = (bounds.width - _stageWidth * scale) / 2, top = (bounds.height - _stageHeight * scale) / 2;
    CGPoint point = [touch locationInView:self.view];
    return CGPointMake((point.x - left) / scale, (point.y - top) / scale);
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    CGPoint point = [self stagePoint:touches.anyObject];
    _api->runtime_feed_mouse(_runtime, (int32_t)point.x, (int32_t)point.y);
    _api->runtime_feed_mouse_button(_runtime, 1, 1);
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    CGPoint point = [self stagePoint:touches.anyObject];
    _api->runtime_feed_mouse(_runtime, (int32_t)point.x, (int32_t)point.y);
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    CGPoint point = [self stagePoint:touches.anyObject];
    _api->runtime_feed_mouse(_runtime, (int32_t)point.x, (int32_t)point.y);
    _api->runtime_feed_mouse_button(_runtime, 1, 0);
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _api->runtime_feed_mouse_button(_runtime, 1, 0);
}
@end

@interface SceneDelegate : UIResponder <UIWindowSceneDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation SceneDelegate
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session
      options:(UISceneConnectionOptions *)connectionOptions
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
