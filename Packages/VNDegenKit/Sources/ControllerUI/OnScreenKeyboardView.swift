import SwiftUI

extension View {
    /// Shows `keyboard` over the bottom of this view while it is open. Tapping outside it closes it.
    public func onScreenKeyboard(_ keyboard: OnScreenKeyboard?) -> some View {
        overlay {
            if let keyboard {
                ZStack(alignment: .bottom) {
                    Color.black.opacity(0.45)
                        .ignoresSafeArea()
                        .onTapGesture { keyboard.close() }
                    OnScreenKeyboardView(keyboard: keyboard)
                }
                .transition(.opacity)
            }
        }
        .animation(.easeOut(duration: 0.18), value: keyboard.map(ObjectIdentifier.init))
    }
}

/// The text being typed, the keys, and what the controller's buttons do.
struct OnScreenKeyboardView: View {
    let keyboard: OnScreenKeyboard
    @FocusState private var fieldFocused: Bool

    static let keyHeight: CGFloat = 40
    static let spacing: CGFloat = 6

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            field
            keys
            hints
        }
        .padding(16)
        .frame(maxWidth: 720)
        .background(.regularMaterial, in: .rect(cornerRadius: 22, style: .continuous))
        .overlay { RoundedRectangle(cornerRadius: 22, style: .continuous).strokeBorder(.white.opacity(0.12)) }
        .padding(12)
        .environment(\.colorScheme, .dark)
        #if os(macOS)
        // A Mac's keyboard types into the field as well.
        .onAppear { fieldFocused = true }
        #endif
    }

    private var field: some View {
        let text = Binding(get: { keyboard.text }, set: { keyboard.replace(with: $0) })
        return HStack(spacing: 10) {
            Text(keyboard.title).font(.system(size: 14, weight: .semibold)).foregroundStyle(.secondary)
            Group {
                if keyboard.isSecure {
                    SecureField(keyboard.title, text: text)
                } else {
                    TextField(keyboard.title, text: text)
                        .autocorrectionDisabled()
                        #if os(iOS)
                        .textInputAutocapitalization(.never)
                        #endif
                }
            }
            .textFieldStyle(.plain)
            .font(.system(size: 17))
            .focused($fieldFocused)
            .onSubmit { keyboard.press(.done) }
        }
        .padding(.horizontal, 14)
        .frame(minHeight: 44)
        .background(.white.opacity(0.08), in: .rect(cornerRadius: 12, style: .continuous))
    }

    private var keys: some View {
        let rows = keyboard.rows
        return GeometryReader { geo in
            // The width of one character key. A key `span` half units wide also covers the gaps it spans.
            let unit = (geo.size.width - 9 * Self.spacing) / 10
            VStack(spacing: Self.spacing) {
                ForEach(rows.indices, id: \.self) { row in
                    HStack(spacing: Self.spacing) {
                        ForEach(rows[row].indices, id: \.self) { column in
                            let key = rows[row][column]
                            let span = CGFloat(OnScreenKeyboard.span(key)) / 2
                            keyButton(key, focused: keyboard.isFocused(row: row, column: column))
                                .frame(width: span * unit + (span - 1) * Self.spacing, height: Self.keyHeight)
                        }
                    }
                }
            }
        }
        .frame(height: CGFloat(rows.count) * Self.keyHeight + CGFloat(rows.count - 1) * Self.spacing)
    }

    private func keyButton(_ key: OnScreenKeyboard.Key, focused: Bool) -> some View {
        Button { keyboard.tap(key) } label: {
            label(key)
                .font(.system(size: 17, weight: .medium))
                .foregroundStyle(focused ? .black : .white)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .background(focused ? AnyShapeStyle(.white) : AnyShapeStyle(.white.opacity(isAccented(key) ? 0.24 : 0.12)),
                            in: .rect(cornerRadius: 8, style: .continuous))
                .scaleEffect(focused ? 1.06 : 1)
                .animation(.spring(response: 0.22, dampingFraction: 0.8), value: focused)
                .contentShape(.rect)
        }
        .buttonStyle(.plain)
        .accessibilityLabel(accessibilityLabel(key))
    }

    @ViewBuilder
    private func label(_ key: OnScreenKeyboard.Key) -> some View {
        switch key {
        case let .character(character): Text(String(character))
        case .shift: Image(systemName: keyboard.shifted ? "shift.fill" : "shift")
        case .symbols: Text(keyboard.showsSymbols ? "ABC" : "#+=").font(.system(size: 15, weight: .semibold))
        case .space: Text("space").font(.system(size: 15))
        case .delete: Image(systemName: "delete.left")
        case .done: Text("Done").font(.system(size: 15, weight: .bold))
        }
    }

    private func isAccented(_ key: OnScreenKeyboard.Key) -> Bool {
        switch key {
        case .shift: keyboard.shifted
        case .symbols: keyboard.showsSymbols
        case .done: true
        default: false
        }
    }

    private func accessibilityLabel(_ key: OnScreenKeyboard.Key) -> String {
        switch key {
        case let .character(character): String(character)
        case .shift: "Shift"
        case .symbols: keyboard.showsSymbols ? "Letters" : "Symbols"
        case .space: "Space"
        case .delete: "Delete"
        case .done: "Done"
        }
    }

    private var hints: some View {
        HStack(spacing: 14) {
            hint("A", "Type")
            hint("X", "Delete")
            hint("LB", "Shift")
            hint("RB", "Symbols")
            hint("Y", "Done")
            hint("B", "Close")
        }
        .font(.system(size: 12, weight: .semibold))
        .foregroundStyle(.secondary)
        .lineLimit(1)
        .minimumScaleFactor(0.7)
        .frame(maxWidth: .infinity)
    }

    private func hint(_ button: String, _ action: String) -> some View {
        HStack(spacing: 5) {
            Text(button)
                .font(.system(size: 11, weight: .heavy, design: .rounded))
                .foregroundStyle(.white)
                .padding(.horizontal, 6)
                .frame(minWidth: 20, minHeight: 20)
                .background(.white.opacity(0.18), in: .capsule)
            Text(action)
        }
    }
}
