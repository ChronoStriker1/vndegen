import SwiftUI
import VNCore
import EngineRPGMaker
import EngineTyrano
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
    ]
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
    var hub: Hub?
    /// Longest the app waits for an automatic backup before it quits anyway.
    static let quitTimeout: Duration = .seconds(20)
    /// Whether a quit is waiting on backups. AppKit takes exactly one reply to `.terminateLater`.
    private var quitPending = false

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }

    /// Quitting by closing the window, Command-Q, or logging out ends a running game first so its play time is recorded,
    /// then waits for automatic backups, including one still running from a game closed just before. The wait and the
    /// timeout run separately, because a backup stuck on an unreachable share never returns, cancelled or not.
    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        guard let hub else { return .terminateNow }
        guard !quitPending else { return .terminateLater }
        quitPending = true
        Task {
            #if DEBUG
            // `-hangQuitBackup YES` stands in for a backup that never finishes, to test the timeout.
            if UserDefaults.standard.bool(forKey: "hangQuitBackup") { await withUnsafeContinuation { (_: UnsafeContinuation<Void, Never>) in } }
            #endif
            await hub.prepareToQuit()
            finishQuit(sender)
        }
        Task {
            try? await Task.sleep(for: Self.quitTimeout)
            finishQuit(sender)
        }
        return .terminateLater
    }

    private func finishQuit(_ sender: NSApplication) {
        guard quitPending else { return }
        quitPending = false
        sender.reply(toApplicationShouldTerminate: true)
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
