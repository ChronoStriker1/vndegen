import Foundation
import VNCore
import WebKit
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

@MainActor
public enum PageSheet {
    public static func show(_ page: URL, title: String, over view: PlatformView, onWebLink: ((URL) -> Void)? = nil,
                            done: @escaping () -> Void) {
        let controller = PageSheetController(page: page, folder: page.deletingLastPathComponent(), title: title,
                                             onWebLink: onWebLink, done: done)
        #if canImport(UIKit)
        guard let presenter = topController(of: view) else { return done() }
        let navigation = UINavigationController(rootViewController: controller)
        navigation.modalPresentationStyle = .pageSheet
        navigation.presentationController?.delegate = controller
        presenter.present(navigation, animated: true)
        #else
        guard let window = view.window else { return done() }
        let sheet = NSWindow(contentViewController: controller)
        sheet.setContentSize(NSSize(width: min(1100, window.frame.width - 40), height: min(800, window.frame.height - 60)))
        window.beginSheet(sheet) { _ in controller.finish() }
        #endif
    }

    #if canImport(UIKit)
    public static func topController(of view: PlatformView) -> UIViewController? {
        var presenter = view.window?.rootViewController
        while let shown = presenter?.presentedViewController { presenter = shown }
        return presenter
    }
    #endif
}

#if canImport(UIKit)
typealias PageSheetBaseController = UIViewController
#else
typealias PageSheetBaseController = NSViewController
#endif

@MainActor
final class PageSheetController: PageSheetBaseController, WKNavigationDelegate {
    private let page: URL, folder: URL
    private let onWebLink: ((URL) -> Void)?
    private var done: (() -> Void)?
    private let web: WKWebView = {
        let configuration = WKWebViewConfiguration()
        configuration.websiteDataStore = .nonPersistent()
        configuration.allowsAirPlayForMediaPlayback = false
        return WKWebView(frame: .zero, configuration: configuration)
    }()
    private static let offlineRules = #"""
        [{"trigger":{"url-filter":"^https?://"},"action":{"type":"block"}},
         {"trigger":{"url-filter":"^wss?://"},"action":{"type":"block"}},
         {"trigger":{"url-filter":"^ftp://"},"action":{"type":"block"}}]
        """#

    init(page: URL, folder: URL, title: String, onWebLink: ((URL) -> Void)?, done: @escaping () -> Void) {
        self.page = page
        self.folder = folder.standardizedFileURL
        self.onWebLink = onWebLink
        self.done = done
        super.init(nibName: nil, bundle: nil)
        self.title = title
    }

    @available(*, unavailable) required init?(coder: NSCoder) { fatalError("not used") }

    override func loadView() {
        web.navigationDelegate = self
        #if canImport(UIKit)
        view = web
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
        WKContentRuleListStore.default().compileContentRuleList(forIdentifier: "vndegen.manual.offline", encodedContentRuleList: Self.offlineRules) { [weak self] rules, error in
            Task { @MainActor in
                guard let self else { return }
                guard let rules else {
                    frameHostLog.error("page not shown, its offline rule failed: \(String(describing: error), privacy: .public)")
                    return
                }
                self.web.configuration.userContentController.add(rules)
                self.web.loadFileURL(self.page, allowingReadAccessTo: self.folder)
            }
        }
    }

    @objc private func close() {
        #if canImport(UIKit)
        dismiss(animated: true) { self.finish() }
        #else
        if let sheet = view.window, let parent = sheet.sheetParent { parent.endSheet(sheet) }
        #endif
    }

    func finish() {
        done?()
        done = nil
    }

    func webView(_ webView: WKWebView, decidePolicyFor action: WKNavigationAction, decisionHandler: @escaping @MainActor (WKNavigationActionPolicy) -> Void) {
        guard let url = action.request.url else { return decisionHandler(.cancel) }
        if url.isFileURL, url.standardizedFileURL.path.hasPrefix(folder.path + "/") || url.standardizedFileURL == folder {
            return decisionHandler(.allow)
        }
        decisionHandler(.cancel)
        if let onWebLink, ["http", "https"].contains(url.scheme?.lowercased() ?? ""), action.navigationType == .linkActivated {
            onWebLink(url)
        }
    }
}

#if canImport(UIKit)
extension PageSheetController: UIAdaptivePresentationControllerDelegate {
    func presentationControllerDidDismiss(_ presentationController: UIPresentationController) { finish() }
}
#endif
