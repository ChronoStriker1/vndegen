import Testing
@testable import VNCore

private func key(_ code: Int) -> ControlAction { .key(KeyboardKey(windowsKeyCode: code)!) }

@Test func bindingsTakeTheirOwnButtonsFirstThenTheFreeOnesInOrder() throws {
    let suggestion = try #require(ControlSuggestion(placing: [
        .init(key(65)),
        .init(key(83), label: "Skip", on: [.r2]),
        .init(key(68), on: [.l2]),
        .init(key(70)),
        .init(key(71)),
    ], on: [.r2, .view, .l3]))
    #expect(suggestion.buttons == [.r2: key(83), .view: key(65), .l3: key(68)])
    #expect(suggestion.labels == [.r2: "Skip"])
}

@Test func aBindingTakesEveryFreeButtonItNamesAndCarriesUsedOnesOnlyWhenAllowed() throws {
    let suggestion = try #require(ControlSuggestion(placing: [
        .init(key(38), on: [.leftStickUp], carrying: [.up]),
        .init(key(40), on: [], carrying: [], fallback: false),
        .init(key(38)),
    ], on: [.leftStickUp, .x]))
    #expect(suggestion.buttons == [.leftStickUp: key(38), .up: key(38)])
}

@Test func fallbackNeverLandsOnADirectionAndNothingPlacedIsNoSuggestion() {
    #expect(ControlSuggestion(placing: [.init(key(65))], on: [.up, .rightStickLeft]) == nil)
    #expect(ControlSuggestion(placing: [], on: [.x]) == nil)
}

@Test func windowsKeyCodesMakeTheKeysAWebPageReports() {
    #expect(KeyboardKey(windowsKeyCode: 83) == KeyboardKey(key: "s", code: "KeyS", keyCode: 83))
    #expect(KeyboardKey(windowsKeyCode: 17) == KeyboardKey(key: "Control", code: "ControlLeft", keyCode: 17))
    #expect(KeyboardKey(windowsKeyCode: 116) == KeyboardKey(key: "F5", code: "F5", keyCode: 116))
    #expect(KeyboardKey(windowsKeyCode: 51) == KeyboardKey(key: "3", code: "Digit3", keyCode: 51))
    #expect(KeyboardKey(windowsKeyCode: 106) == nil)
}
