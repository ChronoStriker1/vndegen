#if canImport(UIKit)
import SwiftUI
import UIKit
import VNCore

/// Shows the system's on-screen keyboard for a running game, from a button mapped to Show keyboard, and sends what is
/// typed to the game: text, Return, and Backspace. A hardware keyboard types through it too while it shows. Closing the
/// keyboard, with its own key or the button again, gives the keyboard back to the game's view.
struct KeyboardBridge: UIViewRepresentable {
    let session: any EngineSession
    /// The keyboard went away by itself, such as from the iPad keyboard's dismiss key.
    let onDismiss: () -> Void

    func makeUIView(context: Context) -> TypingView {
        TypingView()
    }

    func updateUIView(_ view: TypingView, context: Context) {
        view.type = { [session] in session.type($0) }
        view.onDismiss = onDismiss
        view.focus()
    }

    static func dismantleUIView(_ view: TypingView, coordinator: ()) {
        view.onDismiss = nil
        view.resignFirstResponder()
    }

    /// A text view, so keyboards that compose characters, such as Japanese, work. It holds only text still being
    /// composed, and passes on what the keyboard commits.
    final class TypingView: UITextView, UITextViewDelegate {
        var type: ((TypedText) -> Void)?
        var onDismiss: (() -> Void)?

        init() {
            super.init(frame: .zero, textContainer: nil)
            delegate = self
            backgroundColor = .clear
            textColor = .clear
            tintColor = .clear
            // A game reads plain keystrokes, so nothing rewrites what the player types.
            autocorrectionType = .no
            autocapitalizationType = .none
            spellCheckingType = .no
            smartQuotesType = .no
            smartDashesType = .no
            smartInsertDeleteType = .no
            inlinePredictionType = .no
        }

        required init?(coder: NSCoder) { fatalError("not used") }

        /// Takes the keyboard once the view is in a window, which it may not be yet at its first update.
        func focus() {
            if window != nil, !isFirstResponder { becomeFirstResponder() }
        }

        override func didMoveToWindow() {
            super.didMoveToWindow()
            focus()
        }

        // With nothing being composed, Backspace is the game's.
        override func deleteBackward() {
            if text.isEmpty { type?(.deleteBackward) } else { super.deleteBackward() }
        }

        func textView(_ textView: UITextView, shouldChangeTextIn range: NSRange, replacementText text: String) -> Bool {
            guard text == "\n", markedTextRange == nil else { return true }
            type?(.newline)
            return false
        }

        func textViewDidChange(_ textView: UITextView) {
            guard markedTextRange == nil, !text.isEmpty else { return }
            // Pasted text can hold line breaks, each a press of Return. Windows' "\r\n" is one Character.
            for (index, line) in text.split(omittingEmptySubsequences: false, whereSeparator: \.isNewline).enumerated() {
                if index > 0 { type?(.newline) }
                if !line.isEmpty { type?(.text(String(line))) }
            }
            text = ""
        }

        override func resignFirstResponder() -> Bool {
            guard super.resignFirstResponder() else { return false }
            // After SwiftUI's update, which may be what removed this view.
            if let onDismiss { DispatchQueue.main.async(execute: onDismiss) }
            return true
        }
    }
}
#endif
