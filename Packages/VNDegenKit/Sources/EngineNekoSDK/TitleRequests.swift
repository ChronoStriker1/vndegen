import Foundation
import VNCore
import FrameHost
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

/// What a NekoSDK title asks the host to show: the PC's Version info box, the game's own manual, or a web page that the
/// PC opens in a browser, such as Waffle's survey. A page leaves the app, so it opens only after the player says yes,
/// and never in a session driven by debug hooks or tests, which must not reach the network.
@MainActor
enum TitleRequests {
    static func show(kind: String, payload: String, over view: PlatformView, done: @escaping () -> Void) {
        switch kind {
        case "alert": showMessage(payload, over: view, done: done)
        case "manual": showManual(at: URL(fileURLWithPath: payload), over: view, done: done)
        case "url":
            guard let url = WebLinks.page(payload) else { return done() }
            WebLinks.askToOpen(url, over: view, done: done)
        default: done()
        }
    }

    /// The PC's Version info: its message box's text, with OK.
    static func showMessage(_ text: String, over view: PlatformView, done: @escaping () -> Void) {
        #if canImport(UIKit)
        guard let presenter = PageSheet.topController(of: view) else { return done() }
        let alert = UIAlertController(title: nil, message: text, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "OK", style: .default) { _ in done() })
        presenter.present(alert, animated: true)
        #else
        guard let window = view.window else { return done() }
        let alert = NSAlert()
        alert.messageText = text
        alert.addButton(withTitle: "OK")
        alert.beginSheetModal(for: window) { _ in done() }
        #endif
    }

    /// The game's own manual in a web view that loads only files inside the manual's folder. A web link in it asks to
    /// open in the system browser instead.
    static func showManual(at page: URL, over view: PlatformView, done: @escaping () -> Void) {
        PageSheet.show(page, title: "Manual", over: view, onWebLink: { url in WebLinks.askToOpen(url, over: view) }, done: done)
    }
}
