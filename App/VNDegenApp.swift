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
        library: LibraryStore(engines: engines),
        metadata: MetadataService(),
        input: InputHub(),
        lock: LockService()
    )

    var body: some Scene {
        WindowGroup {
            HubRootView(hub: hub)
                .task {
                    hub.input.start()
                    await hub.library.refresh()
                    // `-autolaunch "<folder name>"` starts a game directly, for testing.
                    if let name = UserDefaults.standard.string(forKey: "autolaunch") {
                        hub.launch(GameID(name))
                    }
                }
        }
    }
}
