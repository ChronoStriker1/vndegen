import Testing
import VNCore
@testable import EngineAliceSoft

/// Before the runtime starts, or after the game ends, the cheats offer the variables and game speed and change nothing.
@MainActor @Test func system3CheatsWithoutARunningGame() async {
    let cheats = System3Cheats { nil }
    #expect(cheats.features == [.variables, .speed])
    #expect(cheats.variableRange == 0...65535)
    await #expect(throws: System3CheatError.self) { try await cheats.state() }
    await #expect(throws: System3CheatError.self) { try await cheats.perform(.setVariable(id: 1, .integer(5))) }
    cheats.apply(CheatSettings())
}

@MainActor @Test func system4CheatsWithoutARunningGame() async {
    let cheats = System4Cheats { nil }
    #expect(cheats.features == [.variables, .speed])
    await #expect(throws: System4CheatError.self) { try await cheats.state() }
    await #expect(throws: System4CheatError.self) { try await cheats.perform(.addGold(100)) }
    cheats.apply(CheatSettings())
}
