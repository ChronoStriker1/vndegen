import Foundation
import VNCore
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

/// The text field a NekoSDK save card's comment is typed in, where the PC game opens its own box on the card. `done`
/// gets the text, or nil when the player cancels.
@MainActor
func askForComment(_ current: String, over view: PlatformView, done: @escaping (String?) -> Void) {
    let title = "Save Comment", message = "Up to 36 letters or 18 Japanese characters, as the PC game takes."
    #if canImport(UIKit)
    var presenter = view.window?.rootViewController
    while let shown = presenter?.presentedViewController { presenter = shown }
    guard let presenter else { return done(nil) }
    let alert = UIAlertController(title: title, message: message, preferredStyle: .alert)
    alert.addTextField { $0.text = current }
    alert.addAction(UIAlertAction(title: "Cancel", style: .cancel) { _ in done(nil) })
    alert.addAction(UIAlertAction(title: "OK", style: .default) { [weak alert] _ in done(alert?.textFields?.first?.text ?? "") })
    presenter.present(alert, animated: true)
    #else
    guard let window = view.window else { return done(nil) }
    let alert = NSAlert()
    alert.messageText = title
    alert.informativeText = message
    let field = NSTextField(string: current)
    field.frame = NSRect(x: 0, y: 0, width: 300, height: 24)
    alert.accessoryView = field
    alert.addButton(withTitle: "OK")
    alert.addButton(withTitle: "Cancel")
    alert.window.initialFirstResponder = field
    alert.beginSheetModal(for: window) { response in done(response == .alertFirstButtonReturn ? field.stringValue : nil) }
    #endif
}
