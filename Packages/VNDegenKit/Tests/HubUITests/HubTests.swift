import Foundation
import GameInput
import Lock
import Metadata
import SaveSync
import Testing
import VNCore
@testable import HubUI

/// A game that keeps quitting until the test lets it stop.
@MainActor
private final class SlowSession: EngineSession {
    let view = PlatformView()
    private var waiters: [CheckedContinuation<Void, Never>] = []
    private(set) var hasStopped = false

    func send(_ event: ControllerEvent) {}
    func setPaused(_ paused: Bool) {}
    func terminate() {}

    func waitUntilStopped() async {
        guard !hasStopped else { return }
        await withCheckedContinuation { waiters.append($0) }
    }

    func stop() {
        hasStopped = true
        waiters.forEach { $0.resume() }
        waiters = []
    }
}

@MainActor
private final class Sessions {
    var made: [SlowSession] = []
    var undoFinished = false
}

private struct SlowPlugin: EnginePlugin {
    let sessions: Sessions

    var descriptor: EngineDescriptor {
        EngineDescriptor(id: EngineID("slow"), displayName: "Slow", shortName: "Slow", platforms: Set(Platform.allCases))
    }

    func detect(_ probe: FolderProbe) -> EngineMatch? {
        probe.contains("slow.txt") ? EngineMatch(engine: descriptor.id, confidence: .certain, evidence: "slow.txt") : nil
    }

    @MainActor func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        let session = SlowSession()
        sessions.made.append(session)
        return session
    }

    func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        [SaveLocation(directory: dataDirectory.appendingPathComponent("saves")) { _ in nil }]
    }
}

@MainActor
@Test func aGameCannotRelaunchOrTouchItsSavesUntilItsLastSessionStopped() async throws {
    let base = FileManager.default.temporaryDirectory.appendingPathComponent("hub-\(UUID().uuidString)", isDirectory: true)
    let paths = LibraryPaths(gamesRoot: base.appendingPathComponent("Games"), support: base.appendingPathComponent("Support"))
    try FileManager.default.createDirectory(at: paths.gamesRoot.appendingPathComponent("Test Game"), withIntermediateDirectories: true)
    try Data().write(to: paths.gamesRoot.appendingPathComponent("Test Game/slow.txt"))
    let sessions = Sessions()
    let library = LibraryStore(paths: paths, engines: [SlowPlugin(sessions: sessions)])
    await library.refresh()
    let id = GameID("Test Game")
    let hub = Hub(library: library, metadata: MetadataService(), input: InputHub(),
                  lock: LockService(keychainService: "vndegen-tests-\(UUID().uuidString)", defaults: UserDefaults(suiteName: UUID().uuidString)!),
                  saveSync: SaveSyncService(library: library))

    hub.launch(id)
    hub.endSession()
    #expect(hub.closing == [id])
    hub.launch(id)
    #expect(sessions.made.count == 1)
    guard case .library = hub.screen else { Issue.record("relaunched while closing"); return }
    #expect(hub.launchError != nil)

    let undo = Task {
        defer { sessions.undoFinished = true }
        try await hub.saveSync.undoRestore(id)
    }
    try await Task.sleep(for: .milliseconds(300))
    #expect(!sessions.undoFinished, "undo waits for the session to stop")

    sessions.made[0].stop()
    await #expect(throws: SaveSyncError.noSnapshot) { try await undo.value }
    await hub.prepareToQuit()
    #expect(hub.closing.isEmpty)
    hub.launchError = nil
    hub.launch(id)
    #expect(sessions.made.count == 2)
}
