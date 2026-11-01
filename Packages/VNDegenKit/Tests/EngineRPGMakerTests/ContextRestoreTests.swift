import Foundation
import Testing
import WebKit

/// Enough of MZ around a real WebGL canvas: a tile layer, an animation sprite, and Effekseer's contexts, effects, and
/// handles, shaped as rmmz_core.js, rmmz_sprites.js, and effekseer.min.js have them. Anything that reaches a released
/// Effekseer context throws, as Effekseer's native calls fail once their context is gone.
private let page = """
    <html><body><script>
    class Context {
        constructor(id, fails) { this.id = id; this.fails = fails; this.nativeptr = id; }
        init() { if (this.fails) throw new Error('no context'); }
        setRestorationOfStatesFlag() {}
        check() { if (!this.nativeptr) throw new Error('released context ' + this.id); }
        loadEffect(name, scale, onload, onerror) { this.check(); return new Effect(this, name, onerror); }
        // Like Effekseer's, this only warns about an effect still waiting for its textures.
        releaseEffect(effect) {
            this.check();
            if (!effect.isLoaded) return;
            effekseer.releasedEffects.push(effect.name);
            effect.nativeptr = null;
        }
        play(effect) { this.check(); return { context: this, get exists() { this.context.check(); return true; }, stop() { this.context.check(); } }; }
    }
    class Effect {
        constructor(context, name, onerror) {
            Object.assign(this, { context, name, onerror, isLoaded: false, nativeptr: 0 });
        }
        // A texture that arrives later reloads the effect in its context.
        _update() { this.context.check(); this.isLoaded = true; this.nativeptr = 1; }
    }
    window.effekseer = {
        made: 0, failNext: false, releasedContexts: [], releasedEffects: [],
        createContext() {
            const fails = this.failNext;
            this.failNext = false;
            return new Context(++this.made, fails);
        },
        releaseContext(context) { this.releasedContexts.push(context.id); context.nativeptr = null; },
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
    window.EffectManager = {
        _cache: {},
        load(name) { return this._cache[name] || (this._cache[name] = Graphics._effekseer && Graphics._effekseer.loadEffect(name)); },
    };
    window.Tilemap = { Layer: function () {} };
    Tilemap.Layer.prototype.render = function () {
        this.uploads = (this.uploads || 0) + (this._needsTexturesUpdate ? 1 : 0);
        this._needsTexturesUpdate = false;
    };
    window.Sprite_Animation = function () { this._handle = null; this._started = false; };
    Sprite_Animation.prototype.setup = function (animation) { this._animation = animation; this._effect = EffectManager.load(animation.effectName); };
    Sprite_Animation.prototype.start = function () { this._handle = Graphics._effekseer.play(this._effect); this._started = true; };
    Sprite_Animation.prototype.update = function () { if (this._handle) this.sawHandle = this._handle.exists; };
    Sprite_Animation.prototype._render = function () { this.drew = !!(this._handle && this._handle.exists); };
    Sprite_Animation.prototype.destroy = function () { if (this._handle) this._handle.stop(); };
    Sprite_Animation.prototype.checkEnd = function () { this.ended = !(this._handle && this._handle.exists); };
    Sprite_Animation.prototype.setRotation = function () {};
    Sprite_Animation.prototype.updateEffectGeometry = function () {};
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
        Graphics._createEffekseerContext();
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
        // One effect has loaded. Another has its native effect and still waits for a texture, and so does one a plugin
        // loaded itself, which keeps the error callback it was given.
        EffectManager.load('slash')._update();
        const pending = EffectManager.load('mist');
        pending.nativeptr = 5;
        let oldError = false;
        const direct = Graphics._effekseer.loadEffect('direct', 1, null, () => { oldError = true; });
        direct.nativeptr = 6;
        const reportError = direct.onerror;
        const playing = new Sprite_Animation(), drawn = new Sprite_Animation(), ended = new Sprite_Animation();
        for (const animation of [playing, drawn, ended]) {
            animation.setup({ effectName: 'slash' });
            animation.start();
        }
        const waiting = new Sprite_Animation();
        waiting.setup({ effectName: 'fire' });
        // Animations a plugin made without MZ's constructor: one runs through no loss, the other is first touched after.
        const steady = Object.create(Sprite_Animation.prototype), late = Object.create(Sprite_Animation.prototype);
        for (const animation of [steady, late]) {
            animation._started = true;
            animation._handle = Graphics._effekseer.play(EffectManager.load('slash'));
        }
        steady.update();
        const app = Graphics._app;
        await restore();
        layer.render();
        layer.render();
        // After the restore, animations are drawn, end, and are checked before any update, and the waiting texture
        // arrives.
        drawn._render();
        ended.destroy();
        late.checkEnd();
        pending._update();
        direct._update();
        reportError();
        playing.update();
        waiting.update();
        const after = Graphics._effekseer.id, cache = Object.keys(EffectManager._cache).sort();
        // When Effekseer cannot start, the game keeps its PIXI application and goes on without it.
        effekseer.failNext = true;
        await restore();
        return {
            uploads: layer.uploads, after, cache, releasedContexts: effekseer.releasedContexts,
            releasedEffects: effekseer.releasedEffects, playing: playing._handle === null, drawn: drawn.drew,
            late: late.ended, steady: steady.sawHandle === true, reloaded: waiting._effect.context.id,
            oldError, keptApp: Graphics._app === app, effekseerAfterFailure: Graphics._effekseer, glError: gl.getError(),
        };
        """, arguments: [:], contentWorld: .page) as? [String: Any]
    let values = try #require(result)
    // The tileset went up once before the loss and once after it, not on every frame.
    #expect(values["uploads"] as? Int == 2)
    #expect(values["after"] as? Int == 2)
    // Only the effect the waiting animation loaded again is cached, in the new context.
    #expect(values["cache"] as? [String] == ["fire"])
    #expect(values["reloaded"] as? Int == 2)
    // The lost context and every effect holding native memory are freed, both times, including effects still waiting
    // for textures and one a plugin loaded. The second context held only 'fire', which had not loaded at all.
    #expect(values["releasedContexts"] as? [Int] == [1, 2])
    #expect(values["releasedEffects"] as? [String] == ["slash", "mist", "direct"])
    // An old request's error arriving after the restore reports nothing.
    #expect(values["oldError"] as? Bool == false)
    #expect(values["playing"] as? Bool == true)
    #expect(values["drawn"] as? Bool == false)
    #expect(values["late"] as? Bool == true)
    #expect(values["steady"] as? Bool == true)
    #expect(values["keptApp"] as? Bool == true)
    #expect(values["effekseerAfterFailure"] is NSNull)
    #expect(values["glError"] as? Int == 0)
}

/// Enough of MV's PIXI 4 around a real WebGL canvas: a renderer that restores its context as pixi.js 4.5.4 does, with a
/// filter manager made once and a pixi-tilemap renderer that makes its index buffer only when its indices grow.
private let mvPage = """
    <html><body><script>
    class Quad { constructor(gl, attribState) { this.attribState = attribState; } }
    const canvas = document.createElement('canvas');
    document.body.appendChild(canvas);
    const gl = canvas.getContext('webgl');
    const tilemap = {
        indices: new Uint16Array(12000), indexBuffer: 'lost buffer',
        onContextChange() {
            if (!this.indices.length) { this.indices = new Uint16Array(12000); this.indexBuffer = 'new buffer'; }
        },
    };
    const renderer = {
        view: canvas, gl, CONTEXT_UID: 0, state: { attribState: {} }, plugins: { tilemap }, removedUnder: [], madeUnder: [],
        filterManager: { quad: new Quad(), shaderCache: { tone: 'lost shader' }, pool: { 1: ['lost target'] } },
        _initContext() { this.madeUnder.push(this.CONTEXT_UID); this.plugins.tilemap.onContextChange(); },
    };
    canvas.addEventListener('webglcontextlost', (event) => event.preventDefault());
    canvas.addEventListener('webglcontextrestored', () => {
        renderer.removedUnder.push(renderer.CONTEXT_UID);
        renderer._initContext();
    });
    window.Graphics = { _renderer: renderer };
    </script></body></html>
    """

@Test @MainActor func aRestoredPixi4ContextRenewsWhatItKeptFromTheLostOne() async throws {
    let source = URL(filePath: #filePath).deletingLastPathComponent()
        .appending(path: "../../Sources/EngineRPGMaker/Resources/rpgmaker.js").standardized
    let configuration = WKWebViewConfiguration()
    for script in ["window.__vndegen = {};", try String(contentsOf: source, encoding: .utf8)] {
        configuration.userContentController.addUserScript(WKUserScript(source: script, injectionTime: .atDocumentStart, forMainFrameOnly: true))
    }
    let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
    webView.loadHTMLString(mvPage, baseURL: nil)
    let deadline = ContinuousClock.now + .seconds(20)
    while try await webView.evaluateJavaScript("document.readyState === 'complete' && !!window.Graphics") as? Bool != true {
        guard ContinuousClock.now < deadline else { throw CancellationError() }
        try await Task.sleep(for: .milliseconds(50))
    }
    let result = try await webView.callAsyncJavaScript("""
        const restore = async () => {
            const lose = gl.getExtension('WEBGL_lose_context');
            const restored = new Promise((resolve) => canvas.addEventListener('webglcontextrestored', resolve, { once: true }));
            lose.loseContext();
            await new Promise((resolve) => setTimeout(resolve, 50));
            lose.restoreContext();
            await restored;
        };
        const quad = renderer.filterManager.quad;
        await restore();
        const first = {
            newQuad: renderer.filterManager.quad !== quad && renderer.filterManager.quad.attribState === renderer.state.attribState,
            shaders: Object.keys(renderer.filterManager.shaderCache).length, pool: Object.keys(renderer.filterManager.pool).length,
            indexBuffer: tilemap.indexBuffer, indices: tilemap.indices.length,
        };
        tilemap.indexBuffer = 'lost again';
        await restore();
        return { ...first, removedUnder: renderer.removedUnder, madeUnder: renderer.madeUnder, indexBufferAgain: tilemap.indexBuffer };
        """, arguments: [:], contentWorld: .page) as? [String: Any]
    let values = try #require(result)
    #expect(values["newQuad"] as? Bool == true)
    #expect(values["shaders"] as? Int == 0)
    #expect(values["pool"] as? Int == 0)
    #expect(values["indexBuffer"] as? String == "new buffer")
    #expect(values["indices"] as? Int == 12000)
    #expect(values["indexBufferAgain"] as? String == "new buffer")
    // PIXI clears its texture records under the old number and makes everything else under a new one, both times.
    #expect(values["removedUnder"] as? [Int] == [0, 1000])
    #expect(values["madeUnder"] as? [Int] == [1000, 2000])
}
