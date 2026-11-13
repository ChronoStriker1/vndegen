import VNCore
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

enum PageDialog {
    enum Kind {
        case alert, confirm
        case prompt(defaultText: String)
    }

    static func show(_ message: String, kind: Kind, over view: PlatformView,
                     done: @escaping @MainActor (_ accepted: Bool, _ text: String?) -> Void) -> (() -> Void)? {
        var answered = false
        let finish = { (accepted: Bool, text: String?) in
            guard !answered else { return }
            answered = true
            done(accepted, text)
        }
        let cancels = if case .alert = kind { false } else { true }
        #if canImport(UIKit)
        var presenter = view.window?.rootViewController
        while let shown = presenter?.presentedViewController { presenter = shown }
        guard let presenter else { return nil }
        let alert = UIAlertController(title: nil, message: message, preferredStyle: .alert)
        if case let .prompt(text) = kind { alert.addTextField { $0.text = text } }
        if cancels { alert.addAction(UIAlertAction(title: "Cancel", style: .cancel) { _ in finish(false, nil) }) }
        alert.addAction(UIAlertAction(title: "OK", style: .default) { [weak alert] _ in
            finish(true, alert?.textFields?.first?.text)
        })
        presenter.present(alert, animated: true)
        return { [weak alert] in
            alert?.dismiss(animated: false)
            finish(false, nil)
        }
        #else
        guard let window = view.window else { return nil }
        let alert = NSAlert()
        alert.messageText = message
        alert.addButton(withTitle: "OK")
        if cancels { alert.addButton(withTitle: "Cancel") }
        var field: NSTextField?
        if case let .prompt(text) = kind {
            let input = NSTextField(string: text)
            input.frame = NSRect(x: 0, y: 0, width: 280, height: 24)
            alert.accessoryView = input
            field = input
        }
        alert.beginSheetModal(for: window) { response in
            finish(response == .alertFirstButtonReturn, field?.stringValue)
        }
        alert.window.initialFirstResponder = field
        return { [weak window, weak alert] in
            if let sheet = alert?.window { window?.endSheet(sheet, returnCode: .alertSecondButtonReturn) }
            finish(false, nil)
        }
        #endif
    }
}
