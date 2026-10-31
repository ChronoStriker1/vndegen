import Foundation
import Testing
import WebKit

/// Enough of MZ around a real WebGL canvas: a tile layer, an animation sprite, and Effekseer's context and effect cache,
/// shaped as rmmz_core.js and rmmz_sprites.js have them.
private let page = """
    <html><body><script>
    window.effekseer = {
        made: 0, released: [],
        createContext() { return { id: ++this.made, init() {}, setRestorationOfStatesFlag() {} }; },
        releaseContext(context) { this.released.push(context.id); },
    };
    const canvas = document.createElement('canvas');
    document.body.appendChild(canvas);
    const gl = canvas.getContext('webgl2') || canvas.getContext('webgl');
    // PIXI asks for the context back, as WebKit restores only a context whose loss the page prevented.
    canvas.addEventListener('webglcontextlost', (event) => event.preventDefault());
    window.Graphics = {
        _app: { view: canvas, renderer: { gl } },
        _createEffekseerContext() { this._effekseer = effekseer.createContext(); this._effekseer.init(gl); },
    };
    Graphics._createEffekseerContext();
    window.EffectManager = { _cache: { 'effects/old.efkefc': {} }, load(name) { return this._cache[name] = { name }; } };
    window.Tilemap = { Layer: function () {} };
    Tilemap.Layer.prototype.render = function () { this.uploads = (this.uploads || 0) + (this._needsTexturesUpdate ? 1 : 0); this._needsTexturesUpdate = false; };
    window.Sprite_Animation = function () {};
    Sprite_Animation.prototype.setup = function (animation) { this._animation = animation; this._effect = EffectManager.load(animation.effectName); };
    Sprite_Animation.prototype.update = function () {};
    </script></body></html>
    """

@Test @MainActor func aRestoredContextReuploadsTilesetsAndRemakesEffekseer() async throws {
    let source = URL(filePath: #filePath).deletingLastPathComponent()
        .appending(path: "../../Sources/EngineRPGMaker/Resources/rpgmaker.js").standardized
    let configuration = WKWebViewConfiguration()
    for script in ["window.__vndegen = {};", try String(contentsOf: source, encoding: .utf8)] {
        configuration.userContentController.addUserScript(WKUserScript(source: script, injectionTime: .atDocumentStart, forMainFrameOnly: true))
    }
    let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
    webView.loadHTMLString(page, baseURL: nil)
    let deadline = ContinuousClock.now + .seconds(20)
    while try await webView.evaluateJavaScript("document.readyState === 'complete' && !!window.Graphics") as? Bool != true {
        guard ContinuousClock.now < deadline else { throw CancellationError() }
        try await Task.sleep(for: .milliseconds(50))
    }
    let result = try await webView.callAsyncJavaScript("""
        // The engine's scripts finish loading, which is when rpgmaker.js wraps what they define.
        document.dispatchEvent(new Event('load'));
        const layer = new Tilemap.Layer();
        layer._needsTexturesUpdate = true;
        layer.render();
        const playing = new Sprite_Animation(), waiting = new Sprite_Animation();
        playing.setup({ effectName: 'slash' });
        playing._started = true;
        playing._handle = { exists: true };
        waiting.setup({ effectName: 'fire' });
        const before = Graphics._effekseer.id;
        const lose = gl.getExtension('WEBGL_lose_context');
        const restored = new Promise((resolve) => canvas.addEventListener('webglcontextrestored', resolve));
        lose.loseContext();
        await new Promise((resolve) => setTimeout(resolve, 50));
        lose.restoreContext();
        await restored;
        layer.render();
        layer.render();
        playing.update();
        waiting.update();
        return {
            uploads: layer.uploads, before, after: Graphics._effekseer.id, released: effekseer.released,
            cache: Object.keys(EffectManager._cache).sort(), handle: playing._handle === null,
            reloaded: waiting._effect !== null && waiting._effect.name === 'fire', glError: gl.getError(),
        };
        """, arguments: [:], contentWorld: .page) as? [String: Any]
    let values = try #require(result)
    // The tileset went up once before the loss and once after it, not on every frame.
    #expect(values["uploads"] as? Int == 2)
    #expect(values["before"] as? Int == 1)
    #expect(values["after"] as? Int == 2)
    #expect(values["released"] as? [Int] == [1])
    // Only the effect the waiting animation loaded again is cached, in the new context.
    #expect(values["cache"] as? [String] == ["fire"])
    #expect(values["handle"] as? Bool == true)
    #expect(values["reloaded"] as? Bool == true)
    #expect(values["glError"] as? Int == 0)
}
