import Foundation
import VNCore
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

@MainActor
public enum WebLinks {
    public static var mayOpenPages: Bool { UserDefaults.standard.string(forKey: "debugHooks") == nil && !underTest }

    public static var underTest: Bool {
        let process = ProcessInfo.processInfo, environment = process.environment
        return NSClassFromString("XCTestCase") != nil
            || ["XCTestConfigurationFilePath", "XCTestBundlePath", "XCTestSessionIdentifier"].contains { environment[$0] != nil }
            || ["xctest", "swiftpm-testing-helper"].contains(process.processName)
    }

    public nonisolated static func page(_ text: String, mail: Bool = false) -> URL? {
        guard let url = URL(string: text), let scheme = url.scheme?.lowercased() else { return nil }
        if mail, scheme == "mailto" {
            return URLComponents(url: url, resolvingAgainstBaseURL: false)?.path.isEmpty == false ? url : nil
        }
        guard ["http", "https"].contains(scheme), url.host?.isEmpty == false else { return nil }
        return url
    }

    public static func askToOpen(_ url: URL, over view: PlatformView, done: @escaping () -> Void = {}) {
        guard page(url.absoluteString, mail: true) != nil else { return done() }
        let mail = url.scheme?.lowercased() == "mailto"
        let title = mail ? "Write an email in your mail app?" : "Open this page in your browser?", message = url.absoluteString
        let open = {
            if mayOpenPages {
                #if canImport(UIKit)
                UIApplication.shared.open(url)
                #else
                NSWorkspace.shared.open(url)
                #endif
            } else {
                frameHostLog.notice("page not opened in a debug or test session: \(url.absoluteString, privacy: .public)")
            }
        }
        #if canImport(UIKit)
        guard let presenter = PageSheet.topController(of: view) else { return done() }
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
}
