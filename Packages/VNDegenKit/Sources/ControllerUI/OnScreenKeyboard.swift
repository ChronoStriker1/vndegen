import Observation
import VNCore

/// A keyboard a controller types on, for text fields. The system keyboard on iPhone and iPad takes no controller input,
/// and a Mac's text fields take only a hardware keyboard, so a model that owns text fields opens one of these when a
/// controller activates a field, and its view draws it with `onScreenKeyboard(_:)`. The host routes controller buttons
/// to `handle(_:)` while it is open.
///
/// A types the focused key, X deletes, LB shifts the next letter, RB switches to symbols, Y and Menu finish, and B
/// closes. Text is only ever added or deleted at its end. Every change reaches the field at once, so closing keeps it.
@MainActor
@Observable
public final class OnScreenKeyboard {
    public enum Key: Hashable, Sendable {
        case character(Character)
        case shift, symbols, space, delete, done
    }

    public let title: String
    public let isSecure: Bool
    public private(set) var text: String
    /// Shifts the next letter only, as the system keyboard does.
    public private(set) var shifted = false
    public private(set) var showsSymbols = false
    /// Row and column in `rows`.
    public private(set) var focus = (row: 1, column: 0)
    /// A controller opens the keyboard, so the ring starts on. Touching a key hides it until the controller presses again.
    public private(set) var showsFocus = true

    @ObservationIgnored private let onChange: (String) -> Void
    @ObservationIgnored private let onDone: () -> Void
    @ObservationIgnored private let onClose: () -> Void

    /// `onChange` gets the text after every key. `onDone` runs when the player finishes, before `onClose`, which runs
    /// however the keyboard closes. The host drops its keyboard in `onClose`.
    public init(title: String, text: String, isSecure: Bool = false, onChange: @escaping (String) -> Void,
                onDone: @escaping () -> Void = {}, onClose: @escaping () -> Void) {
        self.title = title
        self.text = text
        self.isSecure = isSecure
        self.onChange = onChange
        self.onDone = onDone
        self.onClose = onClose
    }

    static let letters: [[Character]] = ["1234567890", "qwertyuiop", "asdfghjkl/", "zxcvbnm.-_"].map(Array.init)
    static let symbols: [[Character]] = ["1234567890", "!@#$%^&*()", "'\":;,?=+[]", "\\|~`<>{}.-"].map(Array.init)
    static let bottomRow: [Key] = [.shift, .symbols, .space, .delete, .done]

    public var rows: [[Key]] {
        let characters = (showsSymbols ? Self.symbols : Self.letters).map { row in
            row.map { Key.character(shifted && !showsSymbols ? Character($0.uppercased()) : $0) }
        }
        return characters + [Self.bottomRow]
    }

    /// How many half units a key takes of its row's 20. Every row adds up to the same width, so moving up or down lands
    /// on the key under the focused one.
    public static func span(_ key: Key) -> Int {
        switch key {
        case .character: 2
        case .space: 8
        case .shift, .symbols, .delete, .done: 3
        }
    }

    public var focusedKey: Key { rows[focus.row][focus.column] }

    /// Returns false for buttons the keyboard does not use, which it ignores while it is open anyway.
    @discardableResult
    public func handle(_ button: VirtualButton) -> Bool {
        showsFocus = true
        switch button {
        case .up: moveVertically(by: -1)
        case .down: moveVertically(by: 1)
        case .left: focus.column = max(0, focus.column - 1)
        case .right: focus.column = min(rows[focus.row].count - 1, focus.column + 1)
        case .confirm: press(focusedKey)
        case .cancel: close()
        case .dash: press(.delete)
        case .menu: press(.done)
        case .pageUp: press(.shift)
        case .pageDown: press(.symbols)
        case .hub: return false
        }
        return true
    }

    /// A key pressed by touch, a pointer, or the controller.
    public func press(_ key: Key) {
        switch key {
        case let .character(character):
            type(String(character))
            shifted = false
        case .space: type(" ")
        case .delete:
            guard !text.isEmpty else { return }
            text.removeLast()
            onChange(text)
        case .shift: shifted.toggle()
        case .symbols: showsSymbols.toggle()
        case .done:
            onDone()
            close()
        }
    }

    /// Closes the keyboard, keeping what was typed.
    public func close() { onClose() }

    /// Text from a hardware keyboard, through the text field the keyboard shows.
    public func replace(with text: String) {
        guard text != self.text else { return }
        self.text = text
        onChange(text)
    }

    public func isFocused(row: Int, column: Int) -> Bool { showsFocus && focus == (row, column) }

    /// A key touched or clicked: it types, and the ring hides.
    public func tap(_ key: Key) {
        showsFocus = false
        press(key)
    }

    private func type(_ characters: String) {
        text += characters
        onChange(text)
    }

    private func moveVertically(by step: Int) {
        let rows = rows
        let next = focus.row + step
        guard rows.indices.contains(next) else { return }
        let row = rows[focus.row]
        // Twice the middle of the focused key, so it stays a whole number.
        let middle = 2 * row.prefix(focus.column).reduce(0) { $0 + Self.span($1) } + Self.span(row[focus.column])
        var end = 0
        for (column, key) in rows[next].enumerated() {
            end += Self.span(key)
            if middle < 2 * end || column == rows[next].count - 1 {
                focus = (next, column)
                return
            }
        }
    }
}
