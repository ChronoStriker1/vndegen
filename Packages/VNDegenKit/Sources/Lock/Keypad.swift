import SwiftUI
import VNCore

/// The steps for creating, changing, or removing a passcode.
nonisolated struct PasscodeFlow: Equatable {
    enum Purpose: String, Identifiable { case create, change, remove; var id: Self { self } }
    enum Step: Equatable { case current, new, confirm(String) }
    enum Outcome: Equatable {
        case next
        case retry(String)
        /// The flow is complete. Carries the new passcode, or nil to remove it.
        case finished(String?)
    }

    let purpose: Purpose
    private(set) var step: Step

    init(_ purpose: Purpose) {
        self.purpose = purpose
        step = purpose == .create ? .new : .current
    }

    var title: String {
        switch step {
        case .current: "Enter your current passcode"
        case .new: "Choose a passcode of 4 to 8 digits"
        case .confirm: "Enter the new passcode again"
        }
    }

    func expectedLength(current: Int?) -> Int? {
        switch step {
        case .current: current
        case .new: nil
        case let .confirm(first): first.count
        }
    }

    mutating func submit(_ code: String, isCurrent: (String) -> Bool) -> Outcome {
        switch step {
        case .current:
            guard isCurrent(code) else { return .retry("Wrong passcode") }
            if purpose == .remove { return .finished(nil) }
            step = .new
            return .next
        case .new:
            step = .confirm(code)
            return .next
        case let .confirm(first):
            guard code == first else {
                step = .new
                return .retry("The passcodes did not match. Choose one again.")
            }
            return .finished(code)
        }
    }
}

enum KeypadKey: Hashable {
    case digit(Int), delete, enter
}

/// What a keypad is showing: typed digits, the gamepad focus, the last error, and the flow it belongs to, if any.
@Observable
final class PasscodeEntry {
    static let rows: [[KeypadKey]] = [
        [.digit(1), .digit(2), .digit(3)],
        [.digit(4), .digit(5), .digit(6)],
        [.digit(7), .digit(8), .digit(9)],
        [.delete, .digit(0), .enter],
    ]
    static let lengths = 4...8

    enum Action: Equatable { case none, submit(String), cancel }

    var digits = ""
    var flow: PasscodeFlow?
    var message: String?
    private(set) var focus = (row: 0, column: 0)
    /// The focus ring shows only once a gamepad or keyboard has moved it, so touch users never see it.
    private(set) var showsFocus = false
    /// Counts rejected codes, to drive the shake.
    private(set) var failures = 0

    init(flow: PasscodeFlow? = nil) { self.flow = flow }

    var focusedKey: KeypadKey { Self.rows[focus.row][focus.column] }

    /// Returns a passcode when this key completes one: the last expected digit, or Enter on a code of valid length.
    func press(_ key: KeypadKey, expectedLength: Int?) -> String? {
        switch key {
        case let .digit(value):
            guard digits.count < (expectedLength ?? Self.lengths.upperBound) else { return nil }
            digits.append(String(value))
            return digits.count == expectedLength ? takeDigits() : nil
        case .delete:
            if !digits.isEmpty { digits.removeLast() }
            return nil
        case .enter:
            let complete = expectedLength.map { digits.count == $0 } ?? Self.lengths.contains(digits.count)
            return complete ? takeDigits() : nil
        }
    }

    /// A digit from a hardware keyboard. Focus moves to Enter so Return, which the hub translates to confirm, submits.
    func type(_ digit: Int, expectedLength: Int?) -> String? {
        focus = (3, 2)
        return press(.digit(digit), expectedLength: expectedLength)
    }

    /// Directions move the focus, confirm presses the focused key, and cancel deletes a digit or, with none left, cancels.
    func handle(_ event: ControllerEvent, expectedLength: Int?) -> Action {
        guard case let .button(button, pressed: true) = event else { return .none }
        showsFocus = true
        switch button {
        case .up: focus.row = max(focus.row - 1, 0)
        case .down: focus.row = min(focus.row + 1, Self.rows.count - 1)
        case .left: focus.column = max(focus.column - 1, 0)
        case .right: focus.column = min(focus.column + 1, 2)
        case .confirm: return press(focusedKey, expectedLength: expectedLength).map(Action.submit) ?? .none
        case .cancel:
            if digits.isEmpty { return .cancel }
            digits.removeLast()
        default: break
        }
        return .none
    }

    func reject(_ message: String) {
        self.message = message
        digits = ""
        failures += 1
    }

    func reset() {
        digits = ""
        message = nil
        focus = (0, 0)
        showsFocus = false
    }

    private func takeDigits() -> String {
        defer { digits = "" }
        return digits
    }
}

/// Title, dots, and the 3 by 4 keypad. Touch presses keys, a hardware keyboard types digits, and the owner forwards
/// gamepad events to `PasscodeEntry.handle`.
struct PasscodeKeypad: View {
    let entry: PasscodeEntry
    let title: String
    let expectedLength: Int?
    let keySize: CGFloat
    let onSubmit: (String) -> Void
    @FocusState private var keyboardFocus: Bool

    var body: some View {
        VStack(spacing: keySize * 0.22) {
            Text(title)
                .font(.title3.weight(.semibold))
                .multilineTextAlignment(.center)
            dots
            Text(entry.message ?? " ")
                .font(.footnote.weight(.medium))
                .foregroundStyle(Color(red: 1, green: 0.55, blue: 0.5))
            Grid(horizontalSpacing: keySize * 0.32, verticalSpacing: keySize * 0.2) {
                ForEach(PasscodeEntry.rows.indices, id: \.self) { row in
                    GridRow {
                        ForEach(PasscodeEntry.rows[row].indices, id: \.self) { column in
                            key(PasscodeEntry.rows[row][column], focused: entry.showsFocus && entry.focus == (row, column))
                        }
                    }
                }
            }
        }
        .foregroundStyle(.white)
        .focusable()
        .focused($keyboardFocus)
        .focusEffectDisabled()
        .onKeyPress(characters: .decimalDigits, phases: .down) { press in
            guard let digit = Int(press.characters) else { return .ignored }
            if let code = entry.type(digit, expectedLength: expectedLength) { onSubmit(code) }
            return .handled
        }
        .onAppear { keyboardFocus = true }
    }

    private var dots: some View {
        let count = expectedLength ?? max(PasscodeEntry.lengths.lowerBound, entry.digits.count)
        return HStack(spacing: 14) {
            ForEach(0..<count, id: \.self) { index in
                Circle()
                    .fill(index < entry.digits.count ? .white : .clear)
                    .strokeBorder(.white.opacity(0.7), lineWidth: 1.5)
                    .frame(width: 13, height: 13)
            }
        }
        .modifier(Shake(phase: CGFloat(entry.failures)))
        .animation(.linear(duration: 0.4), value: entry.failures)
    }

    @ViewBuilder
    private func key(_ key: KeypadKey, focused: Bool) -> some View {
        Button {
            if let code = entry.press(key, expectedLength: expectedLength) { onSubmit(code) }
        } label: {
            ZStack {
                switch key {
                case let .digit(value):
                    Circle().fill(.white.opacity(0.12))
                    Text("\(value)").font(.system(size: keySize * 0.4, weight: .regular, design: .rounded))
                case .delete:
                    Image(systemName: "delete.left").font(.system(size: keySize * 0.3))
                case .enter:
                    Image(systemName: "checkmark").font(.system(size: keySize * 0.3, weight: .semibold))
                }
                Circle().strokeBorder(.white, lineWidth: 2.5).opacity(focused ? 1 : 0)
            }
            .frame(width: keySize, height: keySize)
            .contentShape(Circle())
            .scaleEffect(focused ? 1.08 : 1)
            .animation(.easeOut(duration: 0.12), value: focused)
        }
        .buttonStyle(KeyPressStyle())
        .accessibilityLabel(key.accessibilityLabel)
    }
}

private struct KeyPressStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .background(Circle().fill(.white.opacity(configuration.isPressed ? 0.3 : 0)))
    }
}

private struct Shake: GeometryEffect {
    var phase: CGFloat
    var animatableData: CGFloat {
        get { phase }
        set { phase = newValue }
    }

    func effectValue(size: CGSize) -> ProjectionTransform {
        ProjectionTransform(CGAffineTransform(translationX: 10 * sin(phase * .pi * 6), y: 0))
    }
}

private extension KeypadKey {
    var accessibilityLabel: String {
        switch self {
        case let .digit(value): "\(value)"
        case .delete: "Delete"
        case .enter: "Done"
        }
    }
}

/// The dark backdrop both lock screens share.
struct LockBackdrop: View {
    var body: some View {
        LinearGradient(colors: [Color(red: 0.09, green: 0.1, blue: 0.16), Color(red: 0.03, green: 0.03, blue: 0.06)],
                       startPoint: .top, endPoint: .bottom)
            .ignoresSafeArea()
    }
}
