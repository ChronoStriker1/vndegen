import SwiftUI
import VNCore
import EngineRPGMaker
import Metadata
import GameInput
import Lock
import HubUI

/// Every engine plugin the app ships. Add new engines here.
let engines: [any EnginePlugin] = [
    RPGMakerPlugin(.mz),
    RPGMakerPlugin(.mv),
]

@main
struct VNDegenApp: App {
    @State private var hub = Hub(
        library: LibraryStore(paths: libraryPaths, engines: engines),
        metadata: MetadataService(),
        input: InputHub(),
        lock: LockService()
    )

    var body: some Scene {
        WindowGroup {
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
        #if os(macOS)
        .windowStyle(.hiddenTitleBar)
        .defaultSize(width: 1280, height: 800)
        #endif
    }
}

/// The standard library location. In DEBUG, `-libraryRoot <path>` points the app at a separate library, so test runs
/// on a shared Mac never touch ~/Documents/VNDegen.
private var libraryPaths: LibraryPaths {
    #if DEBUG
    if let root = UserDefaults.standard.string(forKey: "libraryRoot") {
        let base = URL(fileURLWithPath: (root as NSString).expandingTildeInPath, isDirectory: true)
        return LibraryPaths(gamesRoot: base.appendingPathComponent("Games"), support: base.appendingPathComponent("Support"))
    }
    #endif
    return .standard
}
