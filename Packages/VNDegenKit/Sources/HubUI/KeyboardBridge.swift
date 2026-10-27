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
        if !view.isFirstResponder { view.becomeFirstResponder() }
    }

    static func dismantleUIView(_ view: TypingView, coordinator: ()) {
        view.onDismiss = nil
        view.resignFirstResponder()
    }

    final class TypingView: UIView, UIKeyInput {
        var type: ((TypedText) -> Void)?
        var onDismiss: (() -> Void)?

        // A game reads plain keystrokes, so nothing rewrites what the player types.
        var autocorrectionType: UITextAutocorrectionType = .no
        var autocapitalizationType: UITextAutocapitalizationType = .none
        var spellCheckingType: UITextSpellCheckingType = .no
        var smartQuotesType: UITextSmartQuotesType = .no
        var smartDashesType: UITextSmartDashesType = .no
        var smartInsertDeleteType: UITextSmartInsertDeleteType = .no
        var inlinePredictionType: UITextInlinePredictionType = .no

        override var canBecomeFirstResponder: Bool { true }

        // Always true, so Backspace reaches the game even though this view holds no text.
        var hasText: Bool { true }

        func insertText(_ text: String) {
            // Return arrives as a newline, on its own.
            for (index, line) in text.split(separator: "\n", omittingEmptySubsequences: false).enumerated() {
                if index > 0 { type?(.newline) }
                if !line.isEmpty { type?(.text(String(line))) }
            }
        }

        func deleteBackward() {
            type?(.deleteBackward)
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
