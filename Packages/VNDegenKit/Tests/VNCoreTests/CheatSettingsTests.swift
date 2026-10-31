import Foundation
import Testing
@testable import VNCore

@Test func cheatSettingsDefaultToOffAndKeepDefaultsForMissingKeys() throws {
    let empty = try JSONDecoder().decode(CheatSettings.self, from: Data("{}".utf8))
    #expect(empty == CheatSettings() && !empty.isActive && empty.summary.isEmpty)
    let some = try JSONDecoder().decode(CheatSettings.self, from: Data(#"{"speed":2,"noEncounters":true}"#.utf8))
    #expect(some.speed == 2 && some.noEncounters && some.experienceRate == 1 && !some.keepsPartyHealthy)
    #expect(some.isActive && some.summary == ["2x speed", "no random encounters"])
}

@Test func aGameFromALibraryWithoutCheatsHasNone() throws {
    var game = Game(id: GameID("Rathi"))
    let old = try JSONEncoder().encode(game)
    #expect(try JSONDecoder().decode(Game.self, from: old).cheats == nil)
    var settings = CheatSettings()
    settings.experienceRate = 5
    game.cheats = settings
    #expect(try JSONDecoder().decode(Game.self, from: JSONEncoder().encode(game)).cheats == settings)
}
