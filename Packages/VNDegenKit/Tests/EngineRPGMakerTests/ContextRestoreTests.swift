import Foundation
import Testing
import WebKit

/// Enough of MZ around a real WebGL canvas: a tile layer, an animation sprite, and Effekseer's context and effect cache,
/// shaped as rmmz_core.js and rmmz_sprites.js have them.
private let page = """
    <html><body><script>
    window.effekseer = {
        made: 0, released: [], failNext: false,
        createContext() {
            const fails = this.failNext;
            this.failNext = false;
            return { id: ++this.made, init() { if (fails) throw new Error('no context'); }, setRestorationOfStatesFlag() {} };
        },
        releaseContext(context) { this.released.push(context.id); },
    };
    const canvas = document.createElement('canvas');
    document.body.appendChild(canvas);
    const gl = canvas.getContext('webgl2') || canvas.getContext('webgl');
    // PIXI asks for the context back, as WebKit restores only a context whose loss the page prevented.
    canvas.addEventListener('webglcontextlost', (event) => event.preventDefault());
    window.Graphics = {
        _app: { view: canvas, renderer: { gl } },
        // As in rmmz_core.js, which gives up on the PIXI application when Effekseer fails to start.
        _createEffekseerContext() {
            try {
                this._effekseer = effekseer.createContext();
                this._effekseer.init(this._app.renderer.gl);
            } catch (e) {
                this._app = null;
            }
        },
    };
    Graphics._createEffekseerContext();
    window.EffectManager = { _cache: { 'effects/old.efkefc': {} }, load(name) { return this._cache[name] = { name }; } };
    window.Tilemap = { Layer: function () {} };
    Tilemap.Layer.prototype.render = function () { this.uploads = (this.uploads || 0) + (this._needsTexturesUpdate ? 1 : 0); this._needsTexturesUpdate = false; };
    // A handle from a released or lost context throws when read, as Effekseer's does once its context is gone.
    window.handle = (context) => ({ get exists() { if (context !== Graphics._effekseer) throw new Error('lost handle'); return true; }, stop() { this.stopped = true; } });
    window.Sprite_Animation = function () { this.initialize(); };
    Sprite_Animation.prototype.initialize = function () { this._handle = null; this._started = false; };
    Sprite_Animation.prototype.setup = function (animation) { this._animation = animation; this._effect = EffectManager.load(animation.effectName); };
    Sprite_Animation.prototype.update = function () { if (this._handle) this.sawHandle = this._handle.exists; };
    Sprite_Animation.prototype._render = function () { this.drew = !!(this._handle && this._handle.exists); };
    Sprite_Animation.prototype.destroy = function () { if (this._handle) this._handle.stop(); };
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
        const restore = async () => {
            const lose = gl.getExtension('WEBGL_lose_context');
            const restored = new Promise((resolve) => canvas.addEventListener('webglcontextrestored', resolve, { once: true }));
            lose.loseContext();
            await new Promise((resolve) => setTimeout(resolve, 50));
            lose.restoreContext();
            await restored;
        };
        const layer = new Tilemap.Layer();
        layer._needsTexturesUpdate = true;
        layer.render();
        const playing = new Sprite_Animation(), drawn = new Sprite_Animation(), ended = new Sprite_Animation();
        const waiting = new Sprite_Animation();
        for (const animation of [playing, drawn, ended]) {
            animation.setup({ effectName: 'slash' });
            animation._started = true;
            animation._handle = handle(Graphics._effekseer);
        }
        const endedHandle = ended._handle;
        waiting.setup({ effectName: 'fire' });
        // An animation a plugin made without MZ's constructor keeps its handle while no context is lost.
        const plugin = Object.create(Sprite_Animation.prototype);
        plugin._started = true;
        plugin._handle = handle(Graphics._effekseer);
        plugin.update();
        const app = Graphics._app;
        await restore();
        layer.render();
        layer.render();
        // After the restore, one animation is drawn and one ends before either is updated.
        drawn._render();
        ended.destroy();
        playing.update();
        waiting.update();
        const after = Graphics._effekseer.id, cache = Object.keys(EffectManager._cache).sort();
        // When Effekseer cannot start, the game keeps its PIXI application and goes on without it.
        effekseer.failNext = true;
        await restore();
        return {
            uploads: layer.uploads, after, released: effekseer.released, cache,
            playing: playing._handle === null, drawn: drawn.drew, stopped: !!endedHandle.stopped,
            plugin: plugin.sawHandle === true && plugin._handle !== null,
            reloaded: waiting._effect !== null && waiting._effect.name === 'fire',
            keptApp: Graphics._app === app, effekseerAfterFailure: Graphics._effekseer, glError: gl.getError(),
        };
        """, arguments: [:], contentWorld: .page) as? [String: Any]
    let values = try #require(result)
    // The tileset went up once before the loss and once after it, not on every frame.
    #expect(values["uploads"] as? Int == 2)
    #expect(values["after"] as? Int == 2)
    // The old context stays as it is, for effects it was still loading.
    #expect(values["released"] as? [Int] == [])
    // Only the effect the waiting animation loaded again is cached, in the new context.
    #expect(values["cache"] as? [String] == ["fire"])
    #expect(values["playing"] as? Bool == true)
    #expect(values["drawn"] as? Bool == false)
    #expect(values["stopped"] as? Bool == false)
    #expect(values["plugin"] as? Bool == true)
    #expect(values["reloaded"] as? Bool == true)
    #expect(values["keptApp"] as? Bool == true)
    #expect(values["effekseerAfterFailure"] is NSNull)
    #expect(values["glError"] as? Int == 0)
}
