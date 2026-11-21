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
    /// Whether a page may open at all: not in tests, and not in a session started with `-debugHooks`.
    static var mayOpenPages: Bool { UserDefaults.standard.string(forKey: "debugHooks") == nil && !underTest }

    /// Tests, however they run: XCTest loaded into the process (it never is in the app), its environment, or Swift's own
    /// test runners. Any one is enough, since the environment alone isn't always set.
    static var underTest: Bool {
        let process = ProcessInfo.processInfo, environment = process.environment
        return NSClassFromString("XCTestCase") != nil
            || ["XCTestConfigurationFilePath", "XCTestBundlePath", "XCTestSessionIdentifier"].contains { environment[$0] != nil }
            || ["xctest", "swiftpm-testing-helper"].contains(process.processName)
    }

    static func show(kind: String, payload: String, over view: PlatformView, done: @escaping () -> Void) {
        switch kind {
        case "alert": showMessage(payload, over: view, done: done)
        case "manual": showManual(at: URL(fileURLWithPath: payload), over: view, done: done)
        case "url":
            guard let url = URL(string: payload), ["http", "https"].contains(url.scheme?.lowercased() ?? "") else { return done() }
            askToOpen(url, over: view, done: done)
        default: done()
        }
    }

    /// The PC's Version info: its message box's text, with OK.
    static func showMessage(_ text: String, over view: PlatformView, done: @escaping () -> Void) {
        #if canImport(UIKit)
        guard let presenter = topController(of: view) else { return done() }
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

    /// Asks before a page opens in the system browser. Cancel is the default button, so a stray Return keeps the app.
    static func askToOpen(_ url: URL, over view: PlatformView, done: @escaping () -> Void) {
        let title = "Open this page in your browser?", message = url.absoluteString
        let open = {
            if mayOpenPages {
                #if canImport(UIKit)
                UIApplication.shared.open(url)
                #else
                NSWorkspace.shared.open(url)
                #endif
            } else {
                nekoSDKLog.notice("page not opened in a debug or test session: \(url.absoluteString, privacy: .public)")
            }
        }
        #if canImport(UIKit)
        guard let presenter = topController(of: view) else { return done() }
        let alert = UIAlertController(title: title, message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "Open", style: .default) { _ in open(); done() })
        let cancel = UIAlertAction(title: "Cancel", style: .cancel) { _ in done() }
        alert.addAction(cancel)
        alert.preferredAction = cancel
        presenter.present(alert, animated: true)
        #else
        guard let window = view.window ?? NSApp.keyWindow else { return done() }
        let alert = NSAlert()
        alert.messageText = title
        alert.informativeText = message
        alert.addButton(withTitle: "Cancel")
        alert.addButton(withTitle: "Open")
        alert.beginSheetModal(for: window) { response in
            if response == .alertSecondButtonReturn { open() }
            done()
        }
        #endif
    }

    /// The game's own manual in a web view that loads only files inside the manual's folder. A web link in it asks to
    /// open in the system browser instead.
    static func showManual(at page: URL, over view: PlatformView, done: @escaping () -> Void) {
        PageSheet.show(page, title: "Manual", over: view, onWebLink: { url in askToOpen(url, over: view) {} }, done: done)
    }

    #if canImport(UIKit)
    static func topController(of view: PlatformView) -> UIViewController? {
        var presenter = view.window?.rootViewController
        while let shown = presenter?.presentedViewController { presenter = shown }
        return presenter
    }
    #endif
}
