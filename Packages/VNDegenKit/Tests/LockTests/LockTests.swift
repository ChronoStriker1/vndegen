import Foundation
import Testing
import VNCore
@testable import Lock

@Test func passcodeHashVerifiesOnlyTheSameCode() {
    let record = PasscodeRecord(passcode: "4821", iterations: 1_000)
    #expect(record.matches("4821"))
    #expect(!record.matches("4820"))
    #expect(!record.matches("48210"))
    #expect(!record.matches(""))
    #expect(record.length == 4)
}

@Test func passcodeHashIsSaltedAndNeverPlain() throws {
    let first = PasscodeRecord(passcode: "123456")
    let second = PasscodeRecord(passcode: "123456")
    #expect(first.salt != second.salt)
    #expect(first.hash != second.hash)
    #expect(first.iterations == PasscodeRecord.defaultIterations)
    let stored = try String(decoding: JSONEncoder().encode(first), as: UTF8.self)
    #expect(!stored.contains("123456"))
}

@Test func passcodeRecordRoundTripsThroughItsStoredForm() throws {
    let record = PasscodeRecord(passcode: "90210", iterations: 1_000)
    let decoded = try JSONDecoder().decode(PasscodeRecord.self, from: JSONEncoder().encode(record))
    #expect(decoded == record)
    #expect(decoded.matches("90210"))
}

@Test func changeFlowChecksCurrentThenConfirmsNew() {
    var flow = PasscodeFlow(.change)
    let isCurrent = { (code: String) in code == "1111" }
    #expect(flow.submit("2222", isCurrent: isCurrent) == .retry("Wrong passcode"))
    #expect(flow.submit("1111", isCurrent: isCurrent) == .next)
    #expect(flow.expectedLength(current: 4) == nil)
    #expect(flow.submit("135790", isCurrent: isCurrent) == .next)
    #expect(flow.expectedLength(current: 4) == 6)
    #expect(flow.submit("135791", isCurrent: isCurrent) == .retry("The passcodes did not match. Choose one again."))
    #expect(flow.step == .new)
    _ = flow.submit("2468", isCurrent: isCurrent)
    #expect(flow.submit("2468", isCurrent: isCurrent) == .finished("2468"))
}

@Test func removeFlowNeedsTheCurrentPasscode() {
    var flow = PasscodeFlow(.remove)
    #expect(flow.submit("0000", isCurrent: { $0 == "1234" }) == .retry("Wrong passcode"))
    #expect(flow.submit("1234", isCurrent: { $0 == "1234" }) == .finished(nil))
}

@MainActor @Test func keypadSubmitsAtTheExpectedLengthAndOnEnterOtherwise() {
    let entry = PasscodeEntry()
    for digit in [1, 2, 3] { #expect(entry.press(.digit(digit), expectedLength: 4) == nil) }
    #expect(entry.press(.digit(4), expectedLength: 4) == "1234")
    #expect(entry.digits.isEmpty)

    for digit in [5, 6, 7] { _ = entry.press(.digit(digit), expectedLength: nil) }
    #expect(entry.press(.enter, expectedLength: nil) == nil)
    _ = entry.press(.digit(8), expectedLength: nil)
    #expect(entry.press(.enter, expectedLength: nil) == "5678")
}

@MainActor @Test func gamepadMovesFocusAndPressesKeys() {
    let entry = PasscodeEntry()
    let press = { (button: VirtualButton) in entry.handle(.button(button, pressed: true), expectedLength: 4) }
    #expect(press(.right) == .none)
    #expect(press(.down) == .none)
    #expect(entry.focusedKey == .digit(5))
    #expect(press(.confirm) == .none)
    #expect(entry.digits == "5")
    #expect(press(.cancel) == .none)
    #expect(entry.digits.isEmpty)
    #expect(press(.cancel) == .cancel)
    #expect(entry.handle(.button(.confirm, pressed: false), expectedLength: 4) == .none)
}
