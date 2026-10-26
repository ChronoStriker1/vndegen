import Foundation
import Testing
import WebKit

private let script = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
    .appendingPathComponent("../../Sources/EngineTyrano/Resources/tyrano.js").standardizedFileURL

/// A screen like a TyranoScript 5 game stopped on a choice the game built from plain HTML, under a Save button that
/// Tyrano's own focus can reach. The stand-in for Tyrano's key_mouse moves its focus only between .tyrano-focusable.
private let page = """
<html><body style="margin:0">
<button id="save" class="tyrano-focusable" tabindex="0" style="position:absolute;left:700px;top:10px;width:80px;height:30px"
    onclick="window.chosen='save'">Save</button>
<div id="choice" onclick="window.chosen='choice'" style="position:absolute;left:300px;top:280px;width:200px;height:40px">Go left</div>
<script>
const all = (selector) => Object.assign(Array.from(document.querySelectorAll(selector)), { toArray() { return Array.from(this); } });
const native = () => all('[tabindex].tyrano-focusable');
// A class stands in for DOM focus, which a web view that is not in a key window never gives.
const focusFirst = () => { native()[0].classList.add('native-focus'); return true; };
window.TYRANO = { kag: { stat: { is_strong_stop: true, visible_menu_button: true }, key_mouse: {
    util: {
        findFocused: () => all('.native-focus'), findFocusable: native,
        unfocus: () => all('.native-focus').forEach((el) => el.classList.remove('native-focus')),
        canClick: () => false, isRemodalDisplayed: () => false, isMenuDisplayed: () => false,
    },
    vmouse: { is_visible: false },
    focus_up: focusFirst, focus_down: focusFirst, focus_left: focusFirst, focus_right: focusFirst, focus_next: focusFirst,
    ok() { const focused = this.util.findFocused(); if (focused.length) { focused[0].click(); return true; } return false; },
    next: () => false, cancel: () => true,
} } };
</script></body></html>
"""

@MainActor private func press(_ view: WKWebView, _ buttons: [String]) async throws -> Any? {
    for button in buttons {
        _ = try await view.evaluateJavaScript("__vndegenTyrano.button('\(button)', true); __vndegenTyrano.button('\(button)', false); 0")
    }
    return try await view.evaluateJavaScript("window.chosen || 'nothing'")
}

/// Loads a page and resumes once it has finished loading.
@MainActor private final class Loader: NSObject, WKNavigationDelegate {
    private var loaded: CheckedContinuation<Void, Never>?

    func load(_ html: String, in view: WKWebView) async {
        view.navigationDelegate = self
        await withCheckedContinuation { loaded = $0; view.loadHTMLString(html, baseURL: URL(string: "https://localhost/")) }
    }

    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
        loaded?.resume()
        loaded = nil
    }
}

@MainActor @Test func directionsReachAChoiceTyranosFocusCannot() async throws {
    let configuration = WKWebViewConfiguration()
    configuration.userContentController.addUserScript(
        WKUserScript(source: try String(contentsOf: script, encoding: .utf8), injectionTime: .atDocumentStart, forMainFrameOnly: true))
    let loader = Loader()
    for (buttons, expected) in [(["down", "down", "confirm"], "choice"), (["down", "confirm"], "save"), (["confirm", "down", "confirm"], "choice")] {
        let view = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
        await loader.load(page, in: view)
        #expect(try await press(view, buttons) as? String == expected, "\(buttons)")
    }
}
