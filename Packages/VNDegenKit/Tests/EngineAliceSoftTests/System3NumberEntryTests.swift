import Testing
import VNCore
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

/// Under the Keyboard and mouse layout a controller's D-pad sends 1 to 4, A Space and B E.
@MainActor @Test func numberBoxTakesTheDefaultControllerLayout() {
    var answers: [Int?] = []
    let box = System3NumberBox(title: "", entry: System3NumberEntry(value: 7, minimum: 1, maximum: 99)) { answers.append($0) }
    box.press(boundKey: 0x31) // D-pad up
    #expect(box.entry.value == 8)
    box.press(boundKey: 0x34) // D-pad right
    #expect(box.entry.value == 18)
    box.press(boundKey: 0x20) // A
    #expect(answers == [18])
    box.press(boundKey: 0x45) // B
    #expect(answers == [18, nil])
}

@MainActor @Test func numberBoxTakesTheOnScreenKeyboard() {
    var answers: [Int?] = []
    let box = System3NumberBox(title: "", entry: System3NumberEntry(value: 7, minimum: 1, maximum: 99)) { answers.append($0) }
    box.type(.text("42"))
    #expect(box.entry.value == 42)
    box.type(.deleteBackward)
    #expect(box.entry.value == 4)
    box.type(.newline)
    #expect(answers == [4])
}

/// Return held on the choice that opened the box repeats while the box is up. That must not press OK.
@Test func aKeyHeldWhenTheBoxOpensRepeatsAsNothing() {
    var router = System3InputRouter()
    #expect(router.hostKey(hidUsage: 40, pressed: true) == [.key(0x0d, down: true)])
    #expect(router.releaseToNumberBox() == [.key(0x0d, down: false)])
    #expect(router.hostKey(hidUsage: 40, pressed: true).isEmpty)
    #expect(router.hostKey(hidUsage: 40, pressed: false).isEmpty)
    #expect(router.hostKey(hidUsage: 40, pressed: true) == [.key(0x0d, down: true)])
}
