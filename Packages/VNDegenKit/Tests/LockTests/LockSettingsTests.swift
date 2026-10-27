import Foundation
import Testing
import VNCore
@testable import Lock

@MainActor
@Test func aControllerSetsAPasscodeFromTheHiddenGamesPage() {
    let lock = LockService(keychainService: "vndegen-tests-\(UUID().uuidString)", defaults: UserDefaults(suiteName: UUID().uuidString)!)
    let model = LockSettingsModel(lock: lock)
    #expect(model.rows.first == [.setPasscode])
    model.entry = PasscodeEntry(flow: PasscodeFlow(.create))
    // 1111 and Enter, a new passcode of any length from 4 to 8.
    for button: VirtualButton in [.confirm, .confirm, .confirm, .confirm, .down, .down, .down, .right, .right, .confirm] {
        #expect(model.handle(button))
    }
    #expect(model.entry?.flow?.step == .confirm("1111"))
    // Back to 1, and the fourth digit of the confirmation submits it.
    for button: VirtualButton in [.up, .up, .up, .left, .left, .confirm, .confirm, .confirm, .confirm] { _ = model.handle(button) }
    // The test runner may have no keychain to save to, which the sheet reports instead of closing.
    if let entry = model.entry {
        #expect(entry.message == "The passcode could not be saved. Try again.")
    } else {
        #expect(lock.hasPasscode)
        #expect(model.rows.first == [.changePasscode])
    }
}

@MainActor
@Test func cancelDeletesADigitThenClosesThePasscodeSheetThenLeavesThePage() {
    let lock = LockService(keychainService: "vndegen-tests-\(UUID().uuidString)", defaults: UserDefaults(suiteName: UUID().uuidString)!)
    let model = LockSettingsModel(lock: lock)
    model.entry = PasscodeEntry(flow: PasscodeFlow(.create))
    #expect(model.handle(.confirm))
    #expect(model.entry?.digits == "1")
    #expect(model.handle(.cancel))
    #expect(model.entry?.digits == "")
    #expect(model.handle(.cancel))
    #expect(model.entry == nil)
    // With no sheet up, cancel is the host's, which closes the page.
    #expect(!model.handle(.cancel))
}
