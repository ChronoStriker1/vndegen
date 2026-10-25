import SwiftUI
import VNCore
import EngineRPGMaker
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
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
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
