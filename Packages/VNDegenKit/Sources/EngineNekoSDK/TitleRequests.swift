import Foundation
import VNCore
import WebKit
#if canImport(UIKit)
import UIKit
typealias PlatformViewController = UIViewController
#else
import AppKit
typealias PlatformViewController = NSViewController
#endif

/// What a NekoSDK title asks the host to show: the PC's Version info box, the game's own manual, or a web page that the
/// PC opens in a browser, such as Waffle's survey. A page leaves the app, so it opens only after the player says yes,
/// and never in a session driven by debug hooks or tests, which must not reach the network.
@MainActor
enum TitleRequests {
    /// Whether a page may open at all: not under XCTest, and not in a session started with `-debugHooks`.
    static var mayOpenPages: Bool {
        UserDefaults.standard.string(forKey: "debugHooks") == nil && ProcessInfo.processInfo.environment["XCTestConfigurationFilePath"] == nil
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
        let folder = page.deletingLastPathComponent()
        let controller = ManualController(page: page, folder: folder, done: done)
        #if canImport(UIKit)
        guard let presenter = topController(of: view) else { return done() }
        let navigation = UINavigationController(rootViewController: controller)
        navigation.modalPresentationStyle = .pageSheet
        // A swipe down closes the sheet too, without Done.
        navigation.presentationController?.delegate = controller
        presenter.present(navigation, animated: true)
        #else
        guard let window = view.window else { return done() }
        let sheet = NSWindow(contentViewController: controller)
        sheet.setContentSize(NSSize(width: min(1100, window.frame.width - 40), height: min(800, window.frame.height - 60)))
        // However the sheet ends, Done or otherwise, the session hears of it once.
        window.beginSheet(sheet) { _ in controller.finish() }
        #endif
    }

    #if canImport(UIKit)
    static func topController(of view: PlatformView) -> UIViewController? {
        var presenter = view.window?.rootViewController
        while let shown = presenter?.presentedViewController { presenter = shown }
        return presenter
    }
    #endif
}

/// A web view of a game's manual, held to the manual's own folder.
@MainActor
final class ManualController: PlatformViewController, WKNavigationDelegate {
    private let page: URL, folder: URL
    private var done: (() -> Void)?
    private let web = WKWebView()

    init(page: URL, folder: URL, done: @escaping () -> Void) {
        self.page = page
        self.folder = folder.standardizedFileURL
        self.done = done
        super.init(nibName: nil, bundle: nil)
    }

    @available(*, unavailable) required init?(coder: NSCoder) { fatalError("not used") }

    override func loadView() {
        web.navigationDelegate = self
        #if canImport(UIKit)
        view = web
        title = "Manual"
        navigationItem.rightBarButtonItem = UIBarButtonItem(barButtonSystemItem: .done, target: self, action: #selector(close))
        #else
        let close = NSButton(title: "Done", target: self, action: #selector(close))
        close.keyEquivalent = "\r"
        let stack = NSStackView(views: [web, close])
        stack.orientation = .vertical
        stack.alignment = .trailing
        stack.edgeInsets = NSEdgeInsets(top: 0, left: 0, bottom: 10, right: 12)
        web.translatesAutoresizingMaskIntoConstraints = false
        web.widthAnchor.constraint(equalTo: stack.widthAnchor).isActive = true
        view = stack
        #endif
        web.loadFileURL(page, allowingReadAccessTo: folder)
    }

    @objc private func close() {
        #if canImport(UIKit)
        dismiss(animated: true) { self.finish() }
        #else
        if let sheet = view.window, let parent = sheet.sheetParent { parent.endSheet(sheet) }
        #endif
    }

    /// Tells the session the manual has closed, once.
    func finish() {
        done?()
        done = nil
    }

    /// Files inside the manual's folder load here; a web link asks to open in the browser; anything else is refused.
    func webView(_ webView: WKWebView, decidePolicyFor action: WKNavigationAction, decisionHandler: @escaping @MainActor (WKNavigationActionPolicy) -> Void) {
        guard let url = action.request.url else { return decisionHandler(.cancel) }
        if url.isFileURL, url.standardizedFileURL.path.hasPrefix(folder.path + "/") || url.standardizedFileURL == folder {
            return decisionHandler(.allow)
        }
        decisionHandler(.cancel)
        if ["http", "https"].contains(url.scheme?.lowercased() ?? ""), action.navigationType == .linkActivated {
            TitleRequests.askToOpen(url, over: view) {}
        }
    }
}

#if canImport(UIKit)
extension ManualController: UIAdaptivePresentationControllerDelegate {
    func presentationControllerDidDismiss(_ presentationController: UIPresentationController) { finish() }
}
#endif
