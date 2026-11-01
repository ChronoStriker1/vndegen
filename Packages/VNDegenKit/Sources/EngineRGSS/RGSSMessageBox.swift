import SwiftUI
import VNCore

/// A message box the game shows with `msgbox`, or `print` and `p` in XP and VX. On PC it is a Windows message box that
/// stops the game until the player presses OK. Here it is drawn over the game, inside the session's view, and the session
/// keeps the game stopped until it closes.
@Observable
final class RGSSMessageBox {
    let title: String
    let text: String
    /// The OK button's frame, normalized over the session's view, so a touch mouse's tap on it counts as pressing it.
    var okFrame = CGRect.zero
    @ObservationIgnored let onOK: () -> Void

    init(title: String, text: String, onOK: @escaping () -> Void) {
        self.title = title
        self.text = text
        self.onOK = onOK
    }
}

struct RGSSMessageBoxView: View {
    let box: RGSSMessageBox
    /// The message's height at the box's width, so the box is as tall as its text, up to half the screen.
    @State private var textHeight: CGFloat = 0
    private static let space = "RGSSMessageBox"

    var body: some View {
        GeometryReader { whole in
            ZStack {
                Color.black.opacity(0.4)
                VStack(alignment: .leading, spacing: 14) {
                    if !box.title.isEmpty {
                        Text(box.title)
                            .font(.headline)
                            .lineLimit(2)
                    }
                    // As tall as the text, up to half the screen; a longer message scrolls.
                    ScrollView {
                        Text(box.text)
                            .font(.body)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .fixedSize(horizontal: false, vertical: true)
                            .onGeometryChange(for: CGFloat.self) { $0.size.height } action: { textHeight = $0 }
                    }
                    .scrollBounceBehavior(.basedOnSize)
                    .frame(height: min(textHeight, whole.size.height * 0.5))
                    HStack {
                        Spacer()
                        Button(action: box.onOK) {
                            Text("OK")
                                .font(.body.weight(.semibold))
                                .frame(minWidth: 72, minHeight: 32)
                        }
                        .buttonStyle(.borderedProminent)
                        // The only button, so a controller's A or B, Return, Space or Escape presses it. The ring says so.
                        .overlay { Capsule().strokeBorder(.white.opacity(0.85), lineWidth: 2).padding(-4) }
                        .keyboardShortcut(.defaultAction)
                        .accessibilityHint("Closes the game's message and lets it go on")
                        .onGeometryChange(for: CGRect.self) { $0.frame(in: .named(Self.space)) } action: { frame in
                            guard whole.size.width > 0, whole.size.height > 0 else { return }
                            box.okFrame = CGRect(x: frame.minX / whole.size.width, y: frame.minY / whole.size.height,
                                                 width: frame.width / whole.size.width,
                                                 height: frame.height / whole.size.height)
                        }
                    }
                }
                .padding(20)
                .frame(maxWidth: min(460, whole.size.width - 32))
                .background(.regularMaterial, in: .rect(cornerRadius: 16, style: .continuous))
                .environment(\.colorScheme, .dark)
            }
            .coordinateSpace(.named(Self.space))
        }
        .ignoresSafeArea()
    }
}
