// RPG Maker MV and MZ additions to the NW.js shim. Injected at document start, right after it.
(function () {
    'use strict';

    // WebKit says it can play Ogg Vorbis, but its decodeAudioData rejects every Ogg file, so the engine would fail on
    // its first sound. Saying no makes MZ decode with its bundled VorbisDecoder and MV pick its .m4a files instead.
    const canPlayType = HTMLMediaElement.prototype.canPlayType;
    HTMLMediaElement.prototype.canPlayType = function (type) {
        return /^audio\/ogg/i.test(String(type)) ? '' : canPlayType.call(this, type);
    };

    // NW.js plays Ogg itself, so desktop MZ never runs its VorbisDecoder, which numbers decoders 0 to 999 and then starts
    // over. Its worker keeps each decoder's state under its number until the page destroys it, and hands the next decoder
    // with that number the old state. A track whose last chunk of a slow load came through _updateBufferOnFetch never
    // tells its decoder it has finished, so that state stays open while the track plays. A thousand sounds later, as
    // footsteps and letter sounds soon make, a new sound's bytes went to it, read from past their end, and stopped the
    // game with "RangeError: length cannot be negative". Numbers that never repeat keep every decoder to its own state.
    // The constructor assigns the counter modulo 1000 and then increments it, so only increments are taken.
    document.addEventListener('load', (event) => {
        if (!/\/vorbisdecoder\.js$/i.test(event.target.src || '') || typeof VorbisDecoder !== 'function') return;
        let next = VorbisDecoder.id || 0;
        Object.defineProperty(VorbisDecoder, 'id', {
            configurable: true,
            get: () => next,
            set: (value) => { if (value > next) next = value; },
        });
    }, true);

    // Reports once which renderer the engine got, so a fall back to PIXI's canvas renderer shows up in the app log.
    // MZ keeps it at Graphics._app.renderer, MV at Graphics._renderer. PIXI's WEBGL renderer type is 1.
    function reportRenderer(attempt) {
        const graphics = window.Graphics;
        const renderer = graphics && ((graphics._app && graphics._app.renderer) || graphics._renderer);
        if (!renderer) {
            if (attempt < 60) setTimeout(() => reportRenderer(attempt + 1), 500);
            return;
        }
        const gl = renderer.gl;
        const info = {
            renderer: renderer.type === 1 ? 'webgl' : 'canvas',
            webglVersion: gl ? (typeof WebGL2RenderingContext !== 'undefined' && gl instanceof WebGL2RenderingContext ? 2 : 1) : 0,
            gpu: gl ? gl.getParameter(gl.RENDERER) : null,
            resolution: renderer.resolution,
        };
        (info.renderer === 'webgl' ? console.info : console.error)('vndegen renderer ' + JSON.stringify(info));
    }
    window.addEventListener('load', () => reportRenderer(0));

    // pixi-filters' tilt-shift takes window.innerWidth and innerHeight, read once when the filter is made, as the size
    // of what it blurs, and blurs more the further a pixel is from its sharp band in that size. NW.js opens the window
    // at the game's resolution, which is what games are tuned for. Here the window is the whole view, so on an iPad the
    // blur spread over half the game, the more so when the game started in portrait. This uses the game's resolution,
    // and moves a band left at its default, half the window's height, to half the game's.
    // The default band can only be told apart on the first apply, while texSize still holds the window's size.
    const fitted = new WeakSet();
    function fitToGame(uniforms) {
        const { texSize, start, end } = uniforms;
        const height = window.Graphics.height;
        if (!fitted.has(uniforms)) {
            fitted.add(uniforms);
            const middle = texSize.y / 2;
            if (start.x === 0 && start.y === middle && end.x === 600 && end.y === middle) start.y = end.y = height / 2;
        }
        texSize.set(window.Graphics.width, height);
    }
    let pixi;
    Object.defineProperty(window, 'PIXI', {
        configurable: true,
        get: () => pixi,
        set(value) {
            if (value === pixi) return;
            pixi = value;
            const filter = value && value.Filter;
            if (!filter) return;
            const apply = filter.prototype.apply;
            filter.prototype.apply = function () {
                const tiltShift = pixi.filters && pixi.filters.TiltShiftAxisFilter;
                if (tiltShift && this instanceof tiltShift && window.Graphics) fitToGame(this.uniforms);
                return apply.apply(this, arguments);
            };
        },
    });

    // A paused session stops the engine's scene loop. On resume, Input.clear drops keys that were down when it paused.
    window.__vndegen.onPause = (paused) => {
        const scenes = window.SceneManager;
        if (paused) {
            if (scenes && scenes.stop) scenes.stop();
        } else {
            if (window.Input && window.Input.clear) window.Input.clear();
            if (scenes && scenes.resume) scenes.resume();
        }
    };
})();
