// RPG Maker MV and MZ additions to the NW.js shim. Injected at document start, right after it.
(function () {
    'use strict';

    // WebKit says it can play Ogg Vorbis, but its decodeAudioData rejects every Ogg file, so the engine would fail on
    // its first sound. Saying no makes MZ decode with its bundled VorbisDecoder and MV pick its .m4a files instead.
    const canPlayType = HTMLMediaElement.prototype.canPlayType;
    HTMLMediaElement.prototype.canPlayType = function (type) {
        return /^audio\/ogg/i.test(String(type)) ? '' : canPlayType.call(this, type);
    };

    // NW.js decodes audio itself, so desktop MZ never runs its bundled VorbisDecoder. Here every sound goes through it,
    // and its faults stopped games on the error screen with "RangeError: length cannot be negative". They are handled
    // below once a class shaped like MZ's decoder appears. The check runs after every script loads, so a renamed or
    // cache-busted decoder file is caught too. MZ makes its first decoder when the first sound plays, long after the
    // decoder's file has loaded.
    const patched = new WeakSet();
    document.addEventListener('load', () => {
        if (typeof WebAudio === 'function' && !patched.has(WebAudio) && typeof WebAudio.prototype._onFetch === 'function'
            && typeof WebAudio.prototype._updateBuffer === 'function') {
            finishFetches(WebAudio);
            patched.add(WebAudio);
        }
        if (typeof VorbisDecoder !== 'function' || patched.has(VorbisDecoder) || !VorbisDecoder.worker
            || typeof VorbisDecoder.prototype.send !== 'function' || typeof VorbisDecoder.prototype.destroy !== 'function') return;
        try {
            numberWithoutRepeats(VorbisDecoder);
        } catch (e) {
            return;
        }
        decodeOtherFormatsInWebKit(VorbisDecoder);
        VorbisDecoder.worker.addEventListener('error', (event) => {
            // MZ stops the game on any uncaught error, and an error in the decoder's worker reaches the page as one.
            // The game goes on instead. The sound being decoded plays at most what was decoded before the error, and
            // what waits for it to end can wait on, such as background music paused for a music effect.
            event.preventDefault();
            console.warn('Ogg decoder: ' + event.message);
        });
        patched.add(VorbisDecoder);
    }, true);

    // Input reads keys and the gamepad as state once per engine update, so a key that went down and up between two
    // updates never counted, and neither did a press that came right after a release. Battles on an iPad simulator ran
    // at about 25 frames a second with two updates per frame, and about a fifth of Enter taps at 12 a second were
    // lost. Key handlers still run as each key event arrives, with every key as it is at that moment, so the engine and
    // its plugins see each event when it happens and can cancel it. What changes is when the button state they set
    // reaches an update: a change to a button whose last change no update has read yet waits for the next update, so
    // every press is down for at least one update and up before the next. A waiting release lets go only if no gamepad
    // button holds the same button. Input.clear drops the changes still waiting, as it drops the button state they
    // would have set. The NW.js shim does the same for the gamepad.
    //
    // VisuMZ's Core Engine, which many MZ games use, stops reading the gamepad for Input.keyRepeatWait frames (0.4 s)
    // after every Input.clear, so that a button held through the clear does not press again. Plugins clear input often:
    // VisuMZ's QTE system clears it as each timed attack starts and on each hit, so every gamepad press in the next
    // 0.4 s was lost, and the touch gamepad and controllers, which reach the game as a gamepad, could not land a chain
    // of timed hits that keyboard and touch players could. Here a clear that starts such a wait ends it at once. New
    // presses count at once, and a button held through the clear stays held and acts again when the wait would have
    // ended, as it did with the wait, so a held direction still scrolls a menu.
    //
    // Plugins replace these methods as they load, so each is wrapped again after every script loads, and the wrappers
    // nest. Only the outermost one acts. Input binds its key handlers when the game starts, after its plugins have
    // loaded.
    const wrapped = new WeakSet();
    // Per button, such as 'ok', with changes waiting: the state updates read now, and the states waiting to follow
    // it. Also the buttons whose last change has reached Input with no update reading it yet.
    const waiting = new Map(), buttonChanged = new Set();
    // Key events already handled by an outer wrapper, which inner ones pass straight on.
    const routed = new WeakSet();
    let updating = 0;
    function wrap(input, name, make) {
        const original = input[name];
        if (typeof original !== 'function' || wrapped.has(original)) return;
        input[name] = make(original);
        wrapped.add(input[name]);
    }
    function padHolds(input, name) {
        return (input._gamepadStates || []).some((pad) => !!pad && pad.some((down, index) => down
            && input.gamepadMapper && input.gamepadMapper[index] === name));
    }
    function keyEvent(handler, input, event) {
        if (routed.has(event)) return handler.call(input, event);
        routed.add(event);
        const name = input.keyMapper && input.keyMapper[event.keyCode];
        const state = input._currentState;
        if (!name || !state) return handler.call(input, event);
        // Handlers see each button as it is now, then updates see what they have not read yet.
        for (const [button, queue] of waiting) state[button] = queue.states[queue.states.length - 1];
        const before = !!state[name];
        let result;
        try {
            result = handler.call(input, event);
        } finally {
            // A handler that cleared Input, as MZ does for Num Lock, left nothing waiting.
            if (input._currentState === state) {
                const after = !!state[name];
                const queue = waiting.get(name);
                if (after !== before) {
                    if (queue) queue.states.push(after);
                    else if (buttonChanged.has(name)) waiting.set(name, { shown: before, states: [after] });
                    else buttonChanged.add(name);
                }
                for (const [button, queue] of waiting) state[button] = queue.shown;
            }
        }
        return result;
    }
    document.addEventListener('load', () => {
        const input = window.Input;
        if (!input) return;
        wrap(input, 'update', (update) => function () {
            if (updating++ === 0 && this._currentState) {
                for (const [name, queue] of waiting) {
                    if (buttonChanged.has(name)) continue;
                    queue.shown = queue.states.shift() || padHolds(this, name);
                    this._currentState[name] = queue.shown;
                    if (!queue.states.length) waiting.delete(name);
                    buttonChanged.add(name);
                }
            }
            try {
                return update.apply(this, arguments);
            } finally {
                if (--updating === 0) buttonChanged.clear();
            }
        });
        wrap(input, '_onKeyDown', (onKeyDown) => function (event) { return keyEvent(onKeyDown, this, event); });
        wrap(input, '_onKeyUp', (onKeyUp) => function (event) { return keyEvent(onKeyUp, this, event); });
        wrap(input, 'clear', (clear) => function () {
            waiting.clear();
            buttonChanged.clear();
            const held = (this._gamepadStates || []).map((state) => (state ? state.slice() : state));
            const result = clear.apply(this, arguments);
            const wait = this._gamepadWait;
            if (wait > 0 && this.gamepadMapper && this._currentState && this._previousState) {
                this._gamepadWait = 0;
                this._gamepadStates = held;
                for (const state of held) {
                    (state || []).forEach((down, index) => {
                        const name = this.gamepadMapper[index];
                        if (!down || !name) return;
                        this._currentState[name] = this._previousState[name] = true;
                        // Input counts _pressedTime up while the latest button is held and presses it at 0, the
                        // update after the wait's last one, as the Core Engine's next read of the gamepad would.
                        this._latestButton = name;
                        this._pressedTime = -wait - 1;
                    });
                }
            }
            return result;
        });
    }, true);

    // MZ's _onFetch tells the decoder a sound has finished loading only when chunks are left over at the end. When
    // _updateBufferOnFetch has just passed the last one on, the decoder never hears it: an Ogg decoder stays open with
    // its last seconds undecoded, and a sound decoded in WebKit, which waits for the whole file, never plays. At the end
    // of a fetch with nothing left over, this passes the complete data on once more, as MZ does when something is left.
    function finishFetches(Audio) {
        const onFetch = Audio.prototype._onFetch;
        Audio.prototype._onFetch = function (response) {
            const body = response.ok && response.body;
            if (body) {
                // The response goes on unchanged, for plugins that override _onFetch too. Only its reader is wrapped.
                // A body or reader that cannot be changed loads as MZ alone would load it.
                const getReader = body.getReader;
                const wrapped = (...args) => {
                    const reader = getReader.apply(body, args);
                    const read = reader.read.bind(reader);
                    try {
                        reader.read = () => read().then((chunk) => {
                            if (chunk.done && !this._isLoaded && this._fetchedSize === 0 && this._data) {
                                this._isLoaded = true;
                                this._updateBuffer();
                                this._data = null;
                            }
                            return chunk;
                        });
                    } catch (e) { /* unchanged reader */ }
                    return reader;
                };
                try { body.getReader = wrapped; } catch (e) { /* unchanged body */ }
            }
            return onFetch.call(this, response);
        };
    }

    // The decoder numbers decoders 0 to 999 and then starts over. Its worker keeps each decoder's state under its number
    // until the page destroys it, and hands the next decoder with that number the old state. A track whose last chunk of
    // a slow load came through _updateBufferOnFetch never tells its decoder it has finished, so that state stays open
    // while the track plays, and a thousand sounds later a new sound's bytes were read from past their end. Numbers that
    // never repeat keep every decoder to its own state. The constructor assigns the counter modulo 1000 and then
    // increments it, so only increments are taken.
    function numberWithoutRepeats(Decoder) {
        let next = Decoder.id || 0;
        Object.defineProperty(Decoder, 'id', {
            configurable: true,
            get: () => next,
            set: (value) => { if (value > next) next = value; },
        });
    }

    // Games ship WAV and MP3 files under .ogg names, which Chromium decodes by their content. The decoder cannot open
    // them, goes on as if it had, and reads offsets from whatever memory that leaves. Data that does not start with
    // Ogg's capture pattern goes to WebKit's decodeAudioData once it has all loaded, and never to the worker.
    function decodeOtherFormatsInWebKit(Decoder) {
        const send = Decoder.prototype.send;
        Decoder.prototype.send = function (arrayBuffer, isLoaded) {
            if (this.vndegenNative === undefined) {
                const head = new Uint8Array(arrayBuffer, 0, Math.min(4, arrayBuffer.byteLength));
                this.vndegenNative = !(head[0] === 0x4f && head[1] === 0x67 && head[2] === 0x67 && head[3] === 0x53)
                    && !!this.context && typeof this.decodeHandler === 'function' && typeof this.errorHandler === 'function';
            }
            if (!this.vndegenNative) return send.apply(this, arguments);
            if (!isLoaded || this.vndegenDecoding) return;
            this.vndegenDecoding = true;
            // A destroyed decoder has left Decoder.instances, and its sound wants nothing more.
            const current = () => Decoder.instances && Decoder.instances[this.id] === this;
            this.context.decodeAudioData(arrayBuffer.slice(0)).then(
                (buffer) => { if (current()) this.decodeHandler(buffer); },
                () => { if (current()) this.errorHandler(); });
        };
    }

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
