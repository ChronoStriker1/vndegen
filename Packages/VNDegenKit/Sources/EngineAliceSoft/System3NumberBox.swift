import SwiftUI
import VNCore

/// What the player has typed into a System 3 game's number box (NI). As on the PC, a value outside the range can't be
/// accepted. The box starts with the game's value selected, so the first digit replaces it.
nonisolated struct System3NumberEntry: Equatable {
    let minimum: Int
    let maximum: Int
    private(set) var text: String
    private var replacesOnDigit = true

    init(value: Int, minimum: Int, maximum: Int) {
        self.minimum = minimum
        self.maximum = maximum
        text = String(value)
    }

    var value: Int? { Int(text) }
    var isValid: Bool { value.map { (minimum...maximum).contains($0) } ?? false }

    mutating func digit(_ digit: Int) {
        if replacesOnDigit || text == "0" { text = "" }
        replacesOnDigit = false
        if text.count < 5 { text += String(digit) } // the runtime's numbers go up to 65535
    }

    mutating func backspace() {
        replacesOnDigit = false
        if !text.isEmpty { text.removeLast() }
    }

    mutating func step(by amount: Int) {
        let stepped = (value ?? minimum) + amount
        text = String(max(minimum, min(maximum, stepped)))
        replacesOnDigit = true
    }
}

/// A number box the game is waiting at. On the PC it is a Windows dialog with a field, a spinner and OK, and Escape
/// cancels. Here it is drawn over the game, inside the session's view, and the runtime stays at the NI until it closes.
@Observable
final class System3NumberBox {
    enum Control: Hashable {
        case digit(Int), backspace, step(Int), ok, cancel
    }

    let title: String
    var entry: System3NumberEntry
    /// Each control's frame, normalized over the session's view, so a touch mouse's tap on one counts as pressing it.
    @ObservationIgnored var frames: [Control: CGRect] = [:]
    @ObservationIgnored let onDone: (Int?) -> Void

    init(title: String, entry: System3NumberEntry, onDone: @escaping (Int?) -> Void) {
        self.title = title
        self.entry = entry
        self.onDone = onDone
    }

    func press(_ control: Control) {
        switch control {
        case let .digit(digit): entry.digit(digit)
        case .backspace: entry.backspace()
        case let .step(amount): entry.step(by: amount)
        case .ok: if entry.isValid { onDone(entry.value) }
        case .cancel: onDone(nil)
        }
    }

    /// The control under a point normalized over the session's view.
    func control(at point: CGPoint) -> Control? {
        frames.first { $0.value.contains(point) }?.key
    }

    /// A key from a hardware keyboard, or one the router made from a control: digits type, the arrows step, Return and
    /// Space are OK, and Escape cancels.
    func press(virtualKey key: Int32) {
        switch key {
        case 0x30...0x39: press(.digit(Int(key) - 0x30))
        case 0x60...0x69: press(.digit(Int(key) - 0x60))
        case 0x08: press(.backspace)
        case 0x26: press(.step(1))
        case 0x28: press(.step(-1))
        case 0x27: press(.step(10))
        case 0x25: press(.step(-10))
        case 0x0d, 0x20: press(.ok)
        case 0x1b: press(.cancel)
        default: break
        }
    }

    /// A key a controller or touch gamepad button is bound to. Under the Keyboard and mouse layout, the one AliceSoft
    /// games start with, the D-pad is 1 to 4, the left stick WASD, A Space and B E, so those mean what the buttons do
    /// rather than what the keys type.
    func press(boundKey key: Int) {
        switch key {
        case 0x31, 0x57: press(.step(1))
        case 0x32, 0x53: press(.step(-1))
        case 0x33, 0x41: press(.step(-10))
        case 0x34, 0x44: press(.step(10))
        case 0x45: press(.cancel)
        default: press(virtualKey: Int32(key))
        }
    }

    func type(_ typed: TypedText) {
        switch typed {
        case let .text(text): text.compactMap(\.wholeNumberValue).filter { (0...9).contains($0) }.forEach { press(.digit($0)) }
        case .newline: press(.ok)
        case .deleteBackward: press(.backspace)
        }
    }
}

struct System3NumberBoxView: View {
    let box: System3NumberBox
    private static let space = "System3NumberBox"

    var body: some View {
        GeometryReader { whole in
            ZStack {
                Color.black.opacity(0.4)
                VStack(alignment: .leading, spacing: 12) {
                    Text(box.title)
                        .font(.headline)
                        .lineLimit(2)
                    HStack(alignment: .top, spacing: 20) {
                        VStack(alignment: .leading, spacing: 12) {
                            Text(box.entry.text.isEmpty ? " " : box.entry.text)
                                .font(.largeTitle.monospacedDigit().weight(.semibold))
                                .frame(maxWidth: .infinity, alignment: .trailing)
                                .padding(.horizontal, 10)
                                .background(.black.opacity(0.35), in: .rect(cornerRadius: 8))
                                .accessibilityLabel("Number")
                            Text("From \(box.entry.minimum) to \(box.entry.maximum)")
                                .font(.footnote)
                                .foregroundStyle(box.entry.isValid ? Color.secondary : Color.red)
                            HStack(spacing: 6) {
                                ForEach([-10, -1, 1, 10], id: \.self) { amount in
                                    key(amount > 0 ? "+\(amount)" : "\(amount)", .step(amount), size: whole.size)
                                }
                            }
                            Spacer(minLength: 0)
                            HStack(spacing: 8) {
                                key("Cancel", .cancel, size: whole.size)
                                key("OK", .ok, size: whole.size, prominent: true)
                                    .disabled(!box.entry.isValid)
                            }
                        }
                        .frame(width: 200)
                        Grid(horizontalSpacing: 6, verticalSpacing: 6) {
                            ForEach([[7, 8, 9], [4, 5, 6], [1, 2, 3]], id: \.self) { row in
                                GridRow {
                                    ForEach(row, id: \.self) { digit in key("\(digit)", .digit(digit), size: whole.size) }
                                }
                            }
                            GridRow {
                                key("0", .digit(0), size: whole.size)
                                    .gridCellColumns(2)
                                key("⌫", .backspace, size: whole.size)
                                    .accessibilityLabel("Delete")
                            }
                        }
                        .frame(width: 156)
                    }
                    .frame(height: 196)
                }
                .padding(18)
                .background(.regularMaterial, in: .rect(cornerRadius: 16, style: .continuous))
                .environment(\.colorScheme, .dark)
            }
            .coordinateSpace(.named(Self.space))
        }
        .ignoresSafeArea()
    }

    private func key(_ title: String, _ control: System3NumberBox.Control, size: CGSize, prominent: Bool = false) -> some View {
        Button { box.press(control) } label: {
            Text(title)
                .font(.body.monospacedDigit().weight(.medium))
                .frame(maxWidth: .infinity, minHeight: 40)
                .contentShape(.rect)
        }
        .buttonStyle(.plain)
        .background(prominent ? Color.accentColor.opacity(0.85) : Color.white.opacity(0.14), in: .rect(cornerRadius: 8))
        .onGeometryChange(for: CGRect.self) { $0.frame(in: .named(Self.space)) } action: { frame in
            guard size.width > 0, size.height > 0 else { return }
            box.frames[control] = CGRect(x: frame.minX / size.width, y: frame.minY / size.height,
                                         width: frame.width / size.width, height: frame.height / size.height)
        }
    }
}
