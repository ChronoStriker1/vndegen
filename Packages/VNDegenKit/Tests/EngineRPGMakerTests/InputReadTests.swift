import Foundation
import Testing
import WebKit

/// A stand-in for MZ's Input, reading keys and one gamepad the way rmmz_core.js does, loaded as a script file as MZ's are.
/// `triggered` lists the buttons that went down in the last update. As in MZ, the latest button pressed counts
/// `_pressedTime` up while held, and `isRepeated` presses it again after `keyRepeatWait` updates.
private let core = """
    var Input = {
        keyRepeatWait: 24, gamepadMapper: { 0: 'ok', 1: 'cancel' }, keyMapper: { 13: 'ok', 32: 'ok', 27: 'cancel', 38: 'up', 16: 'shift' },
        clear() {
            this._currentState = {}; this._previousState = {}; this._gamepadStates = [];
            this._latestButton = null; this._pressedTime = 0;
        },
        update() {
            this._pollGamepads();
            if (this._currentState[this._latestButton]) this._pressedTime++; else this._latestButton = null;
            this.triggered = Object.keys(this._currentState).filter((n) => this._currentState[n] && !this._previousState[n]);
            for (const n of this.triggered) { this._latestButton = n; this._pressedTime = 0; }
            for (const n in this._currentState) this._previousState[n] = this._currentState[n];
        },
        isRepeated(n) {
            return this._currentState[n] && this._latestButton === n
                && (this._pressedTime === 0 || (this._pressedTime >= this.keyRepeatWait && this._pressedTime % 6 === 0));
        },
        _onKeyDown(event) {
            if (event.keyCode === 38 && event.preventDefault) event.preventDefault();
            if (this.keyMapper[event.keyCode]) this._currentState[this.keyMapper[event.keyCode]] = true;
        },
        _onKeyUp(event) { if (this.keyMapper[event.keyCode]) this._currentState[this.keyMapper[event.keyCode]] = false; },
        _pollGamepads() { for (const pad of navigator.getGamepads()) this._updateGamepadState(pad); },
        _updateGamepadState(pad) {
            const last = this._gamepadStates[pad.index] || [], now = pad.buttons.map((b) => b.pressed);
            now.forEach((down, j) => { if (down !== last[j] && this.gamepadMapper[j]) this._currentState[this.gamepadMapper[j]] = down; });
            this._gamepadStates[pad.index] = now;
        },
    };
    Input.clear();
    window.key = (type, keyCode = 13) => Input[type === 'down' ? '_onKeyDown' : '_onKeyUp']({ keyCode: keyCode });
    window.pad = [false, false];
    navigator.getGamepads = () => [{ index: 0, connected: true, buttons: pad.map((p) => ({ pressed: p })) }];
    """

/// A plugin that wraps the key handlers, loaded after the Core Engine.
private let keyPlugin = """
    const down = Input._onKeyDown, up = Input._onKeyUp;
    Input._onKeyDown = function (event) { down.call(this, event); };
    Input._onKeyUp = function (event) { up.call(this, event); };
    """

/// A plugin that notes whether Shift was down when Enter went down, as one handling Shift+Enter would.
private let shiftEnter = """
    const down = Input._onKeyDown;
    Input._onKeyDown = function (event) {
        if (event.keyCode === 13) window.shiftWithEnter = !!this._currentState.shift;
        down.call(this, event);
    };
    """

/// MZ binds the key handlers when the game starts, after its plugins have loaded.
private let start = """
    document.addEventListener('keydown', Input._onKeyDown.bind(Input));
    document.addEventListener('keyup', Input._onKeyUp.bind(Input));
    window.dom = (type, keyCode = 13) => {
        const event = new KeyboardEvent('key' + type, { cancelable: true });
        Object.defineProperty(event, 'keyCode', { value: keyCode });
        document.dispatchEvent(event);
        return event.defaultPrevented;
    };
    """

/// What VisuMZ's Core Engine does to Input: after a clear it reads no gamepad for keyRepeatWait updates.
private let coreEngine = """
    const clear = Input.clear, update = Input.update, poll = Input._pollGamepads;
    Input.clear = function () { clear.call(this); this._gamepadWait = Input.keyRepeatWait; };
    Input.update = function () { update.call(this); if (this._gamepadWait) this._gamepadWait--; };
    Input._pollGamepads = function () { if (this._gamepadWait) return; poll.call(this); };
    """

@MainActor private func gamePage(_ scripts: [String]) async throws -> WKWebView {
    let source = URL(filePath: #filePath).deletingLastPathComponent()
        .appending(path: "../../Sources/EngineRPGMaker/Resources/rpgmaker.js").standardized
    let configuration = WKWebViewConfiguration()
    for script in ["window.__vndegen = {};", try String(contentsOf: source, encoding: .utf8)] {
        configuration.userContentController.addUserScript(WKUserScript(source: script, injectionTime: .atDocumentStart, forMainFrameOnly: true))
    }
    let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 300), configuration: configuration)
    let tags = scripts.map { "<script src=\"data:text/javascript;base64,\(Data($0.utf8).base64EncodedString())\"></script>" }
    webView.loadHTMLString("<html><body>\(tags.joined())</body></html>", baseURL: nil)
    let deadline = ContinuousClock.now + .seconds(20)
    while try await webView.evaluateJavaScript("document.readyState === 'complete' && !!window.Input") as? Bool != true {
        guard ContinuousClock.now < deadline else { throw CancellationError() }
        try await Task.sleep(for: .milliseconds(50))
    }
    return webView
}

/// Runs one engine update after `setup` and returns what went down in it, and whether OK is held.
@MainActor private func update(_ webView: WKWebView, _ setup: String = "") async throws -> String? {
    try await webView.evaluateJavaScript("\(setup); Input.update(); Input.triggered.join() + (Input._currentState.ok ? ' ok held' : '')") as? String
}

@Test @MainActor func gamepadPressesCountRightAfterTheCoreEngineClearsInput() async throws {
    let page = try await gamePage([core, coreEngine])
    #expect(try await update(page, "pad[0] = true") == "ok ok held")
    // Held through a clear, OK stays held and does not press again.
    #expect(try await update(page, "Input.clear()") == " ok held")
    // The next press counts in the next update, where the Core Engine alone would have ignored it for 24.
    #expect(try await update(page, "pad[0] = false; pad[1] = true") == "cancel")
    #expect(try await update(page, "Input.clear(); pad[1] = false") == "")
    #expect(try await update(page, "pad[0] = true") == "ok ok held")
}

@Test @MainActor func aClearWithoutTheCoreEngineWorksAsInMZ() async throws {
    let page = try await gamePage([core])
    #expect(try await update(page, "pad[0] = true") == "ok ok held")
    // MZ itself reads a button held through a clear as a new press.
    #expect(try await update(page, "Input.clear()") == "ok ok held")
}

@Test @MainActor func aKeyTappedBetweenTwoUpdatesStillPresses() async throws {
    let page = try await gamePage([core, coreEngine])
    // The first read of a gamepad after a clear marks all of its buttons up, as MZ's does, so it comes first.
    #expect(try await update(page) == "")
    #expect(try await update(page, "key('down'); key('up')") == "ok ok held")
    #expect(try await update(page) == "")
    // Two taps between updates make two presses, and each change waits for an update to see the one before it.
    #expect(try await update(page, "key('down'); key('up'); key('down'); key('up')") == "ok ok held")
    #expect(try await update(page) == "")
    #expect(try await update(page) == "ok ok held")
    #expect(try await update(page) == "")
    // A press right after a release the game has not seen yet is a new press, not a key that stayed down.
    #expect(try await update(page, "key('down')") == "ok ok held")
    #expect(try await update(page, "key('up'); key('down')") == "")
    #expect(try await update(page) == "ok ok held")
    // A repeat while the key is held changes nothing.
    #expect(try await update(page, "key('down')") == " ok held")
    #expect(try await update(page, "key('up')") == "")
}

@Test @MainActor func keysBoundWhenTheGameStartsLetGoAfterQuickTaps() async throws {
    let page = try await gamePage([core, coreEngine, keyPlugin, start])
    #expect(try await update(page) == "")
    #expect(try await update(page, "dom('down'); dom('up')") == "ok ok held")
    #expect(try await update(page, "dom('down')") == "")
    #expect(try await update(page) == "ok ok held")
    #expect(try await update(page, "dom('up')") == "")
    #expect(try await update(page) == "")
}

@Test @MainActor func aClearDropsKeyChangesStillWaiting() async throws {
    let page = try await gamePage([core, coreEngine])
    #expect(try await update(page) == "")
    // The second tap waits for an update, and the clear comes first.
    #expect(try await update(page, "key('down'); key('up'); key('down'); key('up')") == "ok ok held")
    #expect(try await update(page, "Input.clear()") == "")
    #expect(try await update(page) == "")
}

@Test @MainActor func aHeldGamepadButtonPressesAgainWhenTheCoreEngineWaitWouldHaveEnded() async throws {
    let page = try await gamePage([core, coreEngine])
    #expect(try await update(page, "pad[0] = true") == "ok ok held")
    let repeats = try await page.evaluateJavaScript("""
        Input.clear();
        const seen = [];
        for (let i = 1; i <= 50; i++) { Input.update(); if (Input.isRepeated('ok')) seen.push(i); }
        seen.join()
        """) as? String
    #expect(repeats == "25,49")
}

@Test @MainActor func anotherKeyHeldOnTheSameButtonStaysHeld() async throws {
    let page = try await gamePage([core, coreEngine])
    #expect(try await update(page) == "")
    // Enter and Space are both OK. A tap of Enter, then Space held, is two presses and OK stays down.
    #expect(try await update(page, "key('down'); key('up'); key('down', 32)") == "ok ok held")
    #expect(try await update(page) == "")
    #expect(try await update(page) == "ok ok held")
    #expect(try await update(page) == " ok held")
}

@Test @MainActor func aKeyRepeatDoesNotHideAReleaseStillWaiting() async throws {
    let page = try await gamePage([core, coreEngine])
    #expect(try await update(page) == "")
    #expect(try await update(page, "key('down')") == "ok ok held")
    #expect(try await update(page, "key('up'); key('down'); key('down')") == "")
    #expect(try await update(page) == "ok ok held")
}

@Test @MainActor func keysTheEngineCancelsAreCancelledAsTheyArrive() async throws {
    let page = try await gamePage([core, coreEngine, keyPlugin, start])
    #expect(try await update(page) == "")
    let cancelled = try await page.evaluateJavaScript("[dom('down', 38), dom('up', 38), dom('down', 38)].join()") as? String
    #expect(cancelled == "true,false,true")
}

@Test @MainActor func aKeyReleaseStillWaitingKeepsAHeldGamepadButtonDown() async throws {
    let page = try await gamePage([core, coreEngine])
    #expect(try await update(page) == "")
    // A tap of Enter, then A held on the gamepad: OK stays down after the tap's release reaches an update.
    #expect(try await update(page, "key('down'); key('up'); pad[0] = true") == "ok ok held")
    #expect(try await update(page) == " ok held")
    #expect(try await update(page) == " ok held")
}

@Test @MainActor func keyHandlersSeeKeysAsTheyAreNow() async throws {
    let page = try await gamePage([core, coreEngine, shiftEnter])
    #expect(try await update(page) == "")
    // Shift tapped, then Enter pressed: the plugin sees Shift up, while the next update still sees the Shift tap.
    let seen = try await page.evaluateJavaScript("""
        key('down', 16); key('up', 16); key('down');
        const during = window.shiftWithEnter;
        Input.update();
        [during, Input.triggered.includes('shift'), Input.triggered.includes('ok')].join()
        """) as? String
    #expect(seen == "false,true,true")
}
