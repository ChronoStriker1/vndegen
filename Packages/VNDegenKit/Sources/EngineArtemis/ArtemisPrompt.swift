import ControllerUI
import Observation
import SwiftUI
import VNCore

/// The game's own Yes/No or text dialog, which the script waits on until it is answered. Touch, a mouse, a keyboard, and
/// a controller all answer it. A dialog that asks for text opens the controller keyboard, whose field also takes a
/// hardware keyboard, and a tap on the field brings up the system keyboard.
@Observable
final class ArtemisPrompt {
    let title: String
    let message: String
    let cancels: Bool
    private(set) var keyboard: OnScreenKeyboard?
    @ObservationIgnored private var onAnswer: ((_ accepted: Bool, _ text: String?) -> Void)?

    init(title: String, message: String, cancels: Bool, text: String?, onAnswer: @escaping (Bool, String?) -> Void) {
        self.title = title
        self.message = message
        self.cancels = cancels
        self.onAnswer = onAnswer
        guard let text else { return }
        keyboard = OnScreenKeyboard(title: title.isEmpty ? message : title, text: text) { _ in
        } onDone: { [weak self] in
            self?.answer(true)
        } onClose: { [weak self] in
            // Done has already answered by now. A dialog with no Cancel stays until it is answered.
            if self?.cancels == true { self?.answer(false) }
        }
    }

    func answer(_ accepted: Bool) {
        let onAnswer = onAnswer
        self.onAnswer = nil
        onAnswer?(accepted, accepted ? keyboard?.text : nil)
    }

    func handle(_ event: ControllerEvent) {
        guard let button = Self.button(for: event) else { return }
        if let keyboard {
            keyboard.handle(button)
        } else if button == .confirm {
            answer(true)
        } else if button == .cancel, cancels {
            answer(false)
        }
    }

    /// A key from the keyboard the game's view reads, by virtual-key code. Other keys reach the text field when it has
    /// focus, and do nothing otherwise.
    func handleHostKey(_ key: Int32) {
        if key == 13 { finish() }
        if key == 27, cancels { answer(false) }
    }

    func type(_ typed: TypedText) {
        switch typed {
        case let .text(text): keyboard?.replace(with: (keyboard?.text ?? "") + text)
        case .deleteBackward: keyboard?.press(.delete)
        case .newline: finish()
        }
    }

    private func finish() {
        if let keyboard { keyboard.press(.done) } else { answer(true) }
    }

    /// The button a press stands for. Artemis games start with the "Keyboard and mouse" layout, whose buttons arrive as
    /// keys and the wheel: A, B, X, and Y are Space, E, R, and F, the D-pad is 1 to 4, the left stick is WASD, Menu is
    /// Escape, and LB and RB turn the wheel.
    nonisolated static func button(for event: ControllerEvent) -> VirtualButton? {
        switch event {
        case let .button(button, pressed: true): button
        case let .gamepad(input, pressed: true): input.virtualButton
        case let .key(key, pressed: true): keyButtons[key.code]
        case let .scroll(dy): dy > 0 ? .pageUp : .pageDown
        default: nil
        }
    }

    nonisolated private static let keyButtons: [String: VirtualButton] = [
        "Enter": .confirm, "Space": .confirm, "Escape": .cancel, "KeyE": .cancel, "KeyR": .dash, "KeyF": .menu,
        "Digit1": .up, "Digit2": .down, "Digit3": .left, "Digit4": .right,
        "KeyW": .up, "KeyS": .down, "KeyA": .left, "KeyD": .right,
    ]
}

/// The dialog, drawn over the game.
struct ArtemisPromptView: View {
    let prompt: ArtemisPrompt

    /// How tall the view must be to show the dialog's title and message above the keyboard. An iPhone in landscape has
    /// no room, so there only the keyboard's own label shows, which is the title, or the message when there is no title.
    private static let roomForWords: CGFloat = 520

    var body: some View {
        GeometryReader { geometry in
            if let keyboard = prompt.keyboard {
                ZStack(alignment: .top) {
                    Color.clear.onScreenKeyboard(keyboard)
                    if !prompt.title.isEmpty, !prompt.message.isEmpty, geometry.size.height >= Self.roomForWords {
                        words
                            .padding(20)
                            .frame(maxWidth: 480)
                            .background(panel)
                            .padding(.top, 24)
                            .allowsHitTesting(false)
                    }
                }
            } else {
                choice.frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
        .environment(\.colorScheme, .dark)
    }

    private var choice: some View {
        ZStack {
            // Keeps touches from reaching the game underneath.
            Color.black.opacity(0.45)
                .ignoresSafeArea()
                .contentShape(.rect)
                .onTapGesture {}
            VStack(spacing: 20) {
                words
                HStack(spacing: 12) {
                    if prompt.cancels { answerButton("Cancel", badge: "B", accepted: false) }
                    answerButton("OK", badge: "A", accepted: true)
                }
            }
            .padding(24)
            .frame(maxWidth: 480)
            .background(panel)
            .padding(24)
        }
    }

    private var words: some View {
        VStack(spacing: 8) {
            if !prompt.title.isEmpty { Text(prompt.title).font(.headline) }
            if !prompt.message.isEmpty { Text(prompt.message).font(.body).foregroundStyle(.secondary) }
        }
        .multilineTextAlignment(.center)
    }

    private var panel: some View {
        RoundedRectangle(cornerRadius: 22, style: .continuous)
            .fill(Color(white: 0.1))
            .strokeBorder(.white.opacity(0.12))
            .shadow(color: .black.opacity(0.5), radius: 24)
    }

    private func answerButton(_ label: String, badge: String, accepted: Bool) -> some View {
        Button { prompt.answer(accepted) } label: {
            HStack(spacing: 8) {
                Text(badge)
                    .font(.system(size: 11, weight: .heavy, design: .rounded))
                    .padding(.horizontal, 6)
                    .frame(minWidth: 20, minHeight: 20)
                    .background(.white.opacity(0.18), in: .capsule)
                Text(label).font(.system(size: 17, weight: .semibold))
            }
            .frame(maxWidth: .infinity, minHeight: 44)
            .background(.white.opacity(accepted ? 0.24 : 0.12), in: .rect(cornerRadius: 12, style: .continuous))
            .contentShape(.rect)
        }
        .buttonStyle(.plain)
    }
}
