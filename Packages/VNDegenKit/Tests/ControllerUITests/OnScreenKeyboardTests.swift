import Testing
import VNCore
@testable import ControllerUI

@MainActor
private final class Field {
    var text = ""
    var done = 0
    var closed = 0

    func keyboard(_ initial: String = "", secure: Bool = false) -> OnScreenKeyboard {
        text = initial
        return OnScreenKeyboard(title: "Field", text: initial, isSecure: secure) { [unowned self] in
            text = $0
        } onDone: { [unowned self] in
            done += 1
        } onClose: { [unowned self] in
            closed += 1
        }
    }
}

@MainActor
@Test func theControllerTypesShiftsDeletesAndFinishes() {
    let field = Field()
    let keyboard = field.keyboard("a")
    // Focus starts on q.
    #expect(keyboard.focusedKey == .character("q"))
    keyboard.handle(.pageUp)
    keyboard.handle(.confirm)
    keyboard.handle(.right)
    keyboard.handle(.confirm)
    // Shift lasts one letter.
    #expect(field.text == "aQw")
    keyboard.handle(.dash)
    #expect(field.text == "aQ")
    keyboard.handle(.pageDown)
    #expect(keyboard.focusedKey == .character("@"))
    keyboard.handle(.confirm)
    #expect(field.text == "aQ@")
    keyboard.handle(.menu)
    #expect(field.done == 1)
    #expect(field.closed == 1)
}

@MainActor
@Test func cancelClosesAndKeepsTheText() {
    let field = Field()
    let keyboard = field.keyboard("host")
    keyboard.handle(.confirm)
    keyboard.handle(.cancel)
    #expect(field.text == "hostq")
    #expect(field.done == 0)
    #expect(field.closed == 1)
}

@MainActor
@Test func movingUpAndDownLandsOnTheKeyUnderneath() {
    let field = Field()
    let keyboard = field.keyboard()
    // q, down to a.
    keyboard.handle(.down)
    #expect(keyboard.focusedKey == .character("a"))
    // g, down twice: through b to space, which spans the middle of the row.
    for _ in 0..<4 { keyboard.handle(.right) }
    #expect(keyboard.focusedKey == .character("g"))
    keyboard.handle(.down)
    #expect(keyboard.focusedKey == .character("b"))
    keyboard.handle(.down)
    #expect(keyboard.focusedKey == .space)
    // Space back up lands in the middle of the letters.
    keyboard.handle(.up)
    #expect(keyboard.focusedKey == .character("n"))
    // The last column reaches Done, and Done goes up to the last letter.
    for _ in 0..<9 { keyboard.handle(.right) }
    keyboard.handle(.down)
    #expect(keyboard.focusedKey == .done)
    keyboard.handle(.up)
    #expect(keyboard.focusedKey == .character("_"))
    // Directions stop at the edges.
    for _ in 0..<10 { keyboard.handle(.up) }
    #expect(keyboard.focus.row == 0)
}

@MainActor
@Test func spaceAndDeleteKeysAndAHardwareKeyboardEditTheSameText() {
    let field = Field()
    let keyboard = field.keyboard()
    keyboard.press(.character("a"))
    keyboard.press(.space)
    keyboard.press(.delete)
    keyboard.press(.delete)
    keyboard.press(.delete)
    #expect(field.text == "")
    keyboard.replace(with: "typed")
    #expect(field.text == "typed")
    #expect(keyboard.text == "typed")
}

@MainActor
@Test func touchingAKeyHidesTheFocusRingUntilTheControllerPressesAgain() {
    let field = Field()
    let keyboard = field.keyboard()
    #expect(keyboard.isFocused(row: 1, column: 0))
    keyboard.tap(.character("z"))
    #expect(!keyboard.isFocused(row: 1, column: 0))
    keyboard.handle(.left)
    #expect(keyboard.isFocused(row: 1, column: 0))
}

@MainActor
@Test func everyKeyboardRowIsTheSameWidth() {
    let field = Field()
    let keyboard = field.keyboard()
    for symbols in [false, true] {
        if keyboard.showsSymbols != symbols { keyboard.press(.symbols) }
        for row in keyboard.rows {
            #expect(row.reduce(0) { $0 + OnScreenKeyboard.span($1) } == 20)
        }
    }
}

@Test func theFocusRingRevealsBeforeItMovesAndStaysOnScreen() {
    var ring = FocusRing<String>()
    let rows = [["a"], ["b", "c"]]
    #expect(ring.current(in: rows) == "a")
    let moved = ring.move(.down, in: rows)
    #expect(moved)
    // The first press only shows where focus is.
    #expect(ring.isFocused("a", in: rows))
    _ = ring.move(.down, in: rows)
    _ = ring.move(.right, in: rows)
    #expect(ring.isFocused("c", in: rows))
    let movedOnConfirm = ring.move(.confirm, in: rows)
    #expect(!movedOnConfirm)
    // A target that left the screen falls back to the first.
    #expect(ring.current(in: [["a"]]) == "a")
}
