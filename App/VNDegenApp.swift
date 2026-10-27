import SwiftUI
import VNCore
import EngineRPGMaker
import EngineTyrano
import EngineRenPy
import EngineKiriKiri
import EngineRGSS
import FontPack
import RuntimePackages
import Metadata
import GameInput
import Lock
import SaveSync
import HubUI

/// Every engine plugin the app ships. Add new engines here.
let engines: [any EnginePlugin] = {
    var engines: [any EnginePlugin] = [
        RPGMakerPlugin(.mz),
        RPGMakerPlugin(.mv),
        TyranoPlugin(),
        RenPyPlugin(),
        KiriKiriPlugin(),
    ]
    // RTPs the player installed, kept once for every game.
    let runtimes = RuntimePackageStore(root: libraryPaths.runtimesDirectory)
    for flavor in [RGSSPlugin.Flavor.vxAce, .vx, .xp] {
        engines.append(RGSSPlugin(flavor) { runtimes.installedFolder(for: $0) })
    }
    // The Windows font substitutes every engine reads: the bundled fonts, and the Chinese and Korean ones the player
    // downloaded. Settings configures it again after a download.
    FontPack.configure { runtimes.installedFolder(for: $0) }
    #if DEBUG
    // Registered first, so it wins the MZ engine ID over the real runtime.
    if UserDefaults.standard.bool(forKey: "fakeSession") { engines.insert(FakeSessionPlugin(), at: 0) }
    #endif
    return engines
}()

#if os(macOS)
/// The play session lives in the app's one window, so closing it quits, as closing a PC game's window does.
final class MacAppDelegate: NSObject, NSApplicationDelegate {
    /// Set by the app once its hub exists.
    var hub: Hub? {
        didSet { quit = hub.map(Self.makeQuit) }
    }
    private var quit: AppQuit?
    /// Longest the app waits for an automatic backup before it quits anyway.
    static let quitTimeout: Duration = .seconds(20)

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }

    /// Quitting by closing the window, Command-Q, or logging out ends a running game first so its play time is recorded
    /// and it writes its saves, then waits for automatic backups.
    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        guard let quit else { return .terminateNow }
        let answer = quit.request(fromSystem: Self.systemIsEnding,
                                  reply: { sender.reply(toApplicationShouldTerminate: true) },
                                  quitAgain: { sender.terminate(nil) })
        switch answer {
        case .now: return .terminateNow
        case .later: return .terminateLater
        case .cancel: return .terminateCancel
        }
    }

    /// Whether macOS asked the app to quit because it is logging out, restarting, or shutting down. A quit the player
    /// asked for carries no such reason.
    private static var systemIsEnding: Bool {
        let reasons = [kAELogOut, kAEReallyLogOut, kAEShowRestartDialog, kAERestart, kAEShowShutdownDialog, kAEShutDown]
        guard let event = NSAppleEventManager.shared().currentAppleEvent,
              let reason = event.attributeDescriptor(forKeyword: AEKeyword(kAEQuitReason))?.typeCodeValue else { return false }
        return reasons.map { OSType($0) }.contains(reason)
    }

    private static func makeQuit(_ hub: Hub) -> AppQuit {
        AppQuit(timeout: quitTimeout, frameIsRunning: { KiriKiriPlugin.frameIsRunning }) {
            #if DEBUG
            // `-hangQuitBackup YES` stands in for a backup that never finishes, to test the timeout.
            if UserDefaults.standard.bool(forKey: "hangQuitBackup") { await withUnsafeContinuation { (_: UnsafeContinuation<Void, Never>) in } }
            #endif
            await hub.prepareToQuit()
        }
    }
}
#endif

@main
struct VNDegenApp: App {
    #if os(iOS)
    @UIApplicationDelegateAdaptor(HubAppDelegate.self) private var appDelegate
    #else
    @NSApplicationDelegateAdaptor(MacAppDelegate.self) private var appDelegate
    #endif
    @State private var hub: Hub = {
        let library = LibraryStore(paths: libraryPaths, engines: engines)
        return Hub(library: library, metadata: MetadataService(), input: InputHub(), lock: LockService(),
                   saveSync: SaveSyncService(library: library))
    }()

    var body: some Scene {
        #if os(macOS)
        // One window only. The hub and its play session are app-wide, and a second window would try to host the same
        // native game view. Window, unlike WindowGroup, has no File > New Window.
        Window("VNDegen", id: "main") { root }
            .windowStyle(.hiddenTitleBar)
            .defaultSize(width: 1280, height: 800)
        #else
        WindowGroup { root }
        #endif
    }

    private var root: some View {
        HubRootView(hub: hub)
            .task {
                #if os(macOS)
                appDelegate.hub = hub
                #endif
                hub.input.start()
                #if DEBUG
                await DebugHooks.run(hub)
                #endif
                await hub.refreshLibrary()
                // `-autolaunch "<folder name>"` starts a game directly, for testing.
                if let name = UserDefaults.standard.string(forKey: "autolaunch") {
                    hub.launch(GameID(name))
                }
                #if DEBUG
                await DebugHooks.playInput(hub)
                #endif
            }
    }
}

/// The standard library location. In DEBUG, `-libraryRoot <path>` points the app at a separate library, so test runs
/// on a shared Mac never touch ~/Games/VNDegen.
private var libraryPaths: LibraryPaths {
    #if DEBUG
    if let root = UserDefaults.standard.string(forKey: "libraryRoot") {
        let base = URL(fileURLWithPath: (root as NSString).expandingTildeInPath, isDirectory: true)
        return LibraryPaths(gamesRoot: base.appendingPathComponent("Games"), support: base.appendingPathComponent("Support"))
    }
    #endif
    return .standard
}
