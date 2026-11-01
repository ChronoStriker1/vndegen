import Testing
@testable import EngineAliceSoft

@Test func numberEntryStartsWithTheGameValueSelected() {
    var entry = System3NumberEntry(value: 7, minimum: 1, maximum: 99)
    #expect(entry.isValid)
    entry.digit(4)
    entry.digit(2)
    #expect(entry.value == 42)
}

@Test func numberEntryRefusesOutOfRangeUntilCorrected() {
    var entry = System3NumberEntry(value: 7, minimum: 1, maximum: 99)
    entry.digit(1)
    entry.digit(2)
    entry.digit(3)
    #expect(!entry.isValid)
    entry.backspace()
    #expect(entry.value == 12)
    #expect(entry.isValid)
    entry.backspace()
    entry.backspace()
    #expect(entry.value == nil)
    #expect(!entry.isValid)
}

@Test func numberEntryStepsStayInRange() {
    var entry = System3NumberEntry(value: 95, minimum: 1, maximum: 99)
    entry.step(by: 10)
    #expect(entry.value == 99)
    entry.step(by: -1)
    #expect(entry.value == 98)
    entry.digit(5) // a step selects the value again
    #expect(entry.value == 5)
    entry.step(by: -10)
    #expect(entry.value == 1)
}
