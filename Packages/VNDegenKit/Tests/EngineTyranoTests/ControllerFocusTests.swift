import Foundation
import OSLog
import Testing
import VNCore
import WebKit
@testable import WebRuntime
import EngineTyrano

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

/// The NW.js shim, then this script, as a game session injects them.
@MainActor private func configuration() throws -> WKWebViewConfiguration {
    let game = WebGameConfiguration(folder: GameFolder(root: FileManager.default.temporaryDirectory, webRoot: ""), appName: "test",
                                    buttonScript: { _, _ in nil }, scriptArgument: "test-script", log: Logger())
    let configuration = WKWebViewConfiguration()
    for source in [try WebGameSession.shimSource(for: game), try String(contentsOf: script, encoding: .utf8)] {
        configuration.userContentController.addUserScript(WKUserScript(source: source, injectionTime: .atDocumentStart, forMainFrameOnly: true))
    }
    return configuration
}

@MainActor @Test func directionsReachAChoiceTyranosFocusCannot() async throws {
    let configuration = try configuration()
    let loader = Loader()
    for (buttons, expected) in [(["down", "down", "confirm"], "choice"), (["down", "confirm"], "save"), (["confirm", "down", "confirm"], "choice")] {
        let view = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
        await loader.load(page, in: view)
        #expect(try await press(view, buttons) as? String == expected, "\(buttons)")
    }
}

@MainActor private func pressPad(_ view: WKWebView, _ inputs: [ControlInput]) async throws -> Any? {
    for input in inputs {
        let script = [true, false].map { WebGameSession.gamepadScript(input, pressed: $0) }.joined(separator: ";")
        _ = try await view.evaluateJavaScript(script + "; 0")
    }
    return try await view.evaluateJavaScript("window.chosen || 'nothing'")
}

@MainActor @Test func gamepadButtonsDoTyranoActionsUnlessTheGameBindsThem() async throws {
    let configuration = try configuration()
    let loader = Loader()
    let view = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
    await loader.load(page, in: view)
    #expect(try await pressPad(view, [.down, .leftStickDown, .a]) as? String == "choice")

    // A game that turns on Tyrano's gamepad support but binds nothing still gets VNDegen's actions.
    let unbound = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
    await loader.load(page.replacingOccurrences(of: "stat: {", with: "config: { useGamepad: 'true' }, stat: {"), in: unbound)
    #expect(try await pressPad(unbound, [.down, .down, .a]) as? String == "choice")

    // A button the game's KeyConfig.js binds is the game's alone: here A, which the stand-in engine ignores.
    let bound = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
    await loader.load(page.replacingOccurrences(of: "stat: {", with: "config: { useGamepad: 'true' }, stat: {")
        .replacingOccurrences(of: "key_mouse: {", with: "key_mouse: { map_pad: { button: { A: 'next' }, stick_digital: {}, stick: {} },"), in: bound)
    #expect(try await pressPad(bound, [.down, .down, .a]) as? String == "nothing")
    _ = try await bound.evaluateJavaScript(WebGameSession.gamepadScript(.a, pressed: true) + "; 0")
    #expect(try await bound.evaluateJavaScript("navigator.getGamepads()[0].buttons[0].pressed") as? Bool == true)
}

@Test func legacyRemapsKeepRunningTyranoActions() {
    let plugin = TyranoPlugin()
    #expect(plugin.legacyAction(for: .confirm) == .button(.confirm))
    #expect(plugin.legacyAction(for: .left) == .button(.left))
    #expect(plugin.legacyAction(for: .hub) == nil)
}
