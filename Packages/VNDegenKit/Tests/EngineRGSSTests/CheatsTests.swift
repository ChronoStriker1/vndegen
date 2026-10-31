import Testing
import VNCore
@testable import EngineRGSS

@Test func cheatSettingsBecomeARubyHash() {
    var settings = CheatSettings()
    settings.speed = 4
    settings.noEncounters = true
    #expect(RGSSCheats.ruby(settings) == "{'keepsPartyHealthy' => false, 'noEncounters' => true, 'walksThroughWalls' => false, "
        + "'speed' => 4, 'experienceRate' => 1}")
}

@Test func actionsBecomeRubyHashes() {
    #expect(RGSSCheats.ruby(.addGold(1_000)) == "{'type' => 'gold', 'amount' => 1000}")
    #expect(RGSSCheats.ruby(.addItem(CheatItem.ID(kind: .weapon, number: 3), count: 10))
        == "{'type' => 'item', 'kind' => 'weapon', 'id' => 3, 'count' => 10}")
    #expect(RGSSCheats.ruby(.setVariable(id: 4, .integer(-7))) == "{'type' => 'variable', 'id' => 4, 'value' => -7}")
    #expect(RGSSCheats.ruby(.setSwitch(id: 2, false)) == "{'type' => 'switch', 'id' => 2, 'on' => false}")
}

/// Text a player types stays text, whatever it holds.
@Test func typedTextBecomesCharacterNumbersRubyPacks() {
    #expect(RGSSCheats.ruby("Ab") == "[65,98].pack('U*')")
    #expect(RGSSCheats.ruby("#{exit}") == "[35,123,101,120,105,116,125].pack('U*')")
    #expect(RGSSCheats.ruby("\u{E9}\n") == "[233,10].pack('U*')")
    #expect(RGSSCheats.ruby("") == "[].pack('U*')")
}
