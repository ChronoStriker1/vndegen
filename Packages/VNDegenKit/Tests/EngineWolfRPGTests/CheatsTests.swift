import Testing
import VNCore
@testable import EngineWolfRPG

/// Before the runtime starts, or after the game ends, the cheats offer only what needs no game data and change nothing.
@MainActor @Test func cheatsWithoutARunningGame() async {
    let cheats = WolfRPGCheats { nil }
    #expect(cheats.features == [.variables, .walkThroughWalls, .speed])
    await #expect(throws: WolfRPGCheatError.self) { try await cheats.state() }
    await #expect(throws: WolfRPGCheatError.self) { try await cheats.perform(.addGold(100)) }
    cheats.apply(CheatSettings())
}
