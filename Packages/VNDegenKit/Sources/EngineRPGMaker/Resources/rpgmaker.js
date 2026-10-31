// RPG Maker MV and MZ additions to the NW.js shim. Injected at document start, right after it.
(function () {
    'use strict';

    // WebKit says it can play Ogg Vorbis, but its decodeAudioData rejects every Ogg file, so the engine would fail on
    // its first sound. Saying no makes MZ decode with its bundled VorbisDecoder and MV pick its .m4a files instead. An
    // MV game that ships no .m4a files gets the shared Ogg decoder (ogg-vorbis.js), which decodes Ogg in decodeAudioData,
    // so MV keeps asking for .ogg.
    if (!window.__vndegen.decodesOgg) {
        const canPlayType = HTMLMediaElement.prototype.canPlayType;
        HTMLMediaElement.prototype.canPlayType = function (type) {
            return /^audio\/ogg/i.test(String(type)) ? '' : canPlayType.call(this, type);
        };
    }

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

    // Short looping videos, mostly animated GIFs converted to WebM for picture plugins such as PKD_VPlayer. WebKit takes
    // such a video's length as its last frame's start plus a guess, not the length the file states, so it ends the loop
    // early, then seeks back to the start and shows nothing new for about 300 ms: a 0.56 s loop of four frames took
    // 0.75 s, stuttered at each turn, and fell out of step with the sound made for it. Here the first whole pass of a
    // WebM that states its length is kept frame by frame, and from then on the texture shows those frames as the video
    // plays, over the stated length. PIXI also marked a video texture changed on every tick, so WebKit copied the same
    // frame to the GPU 60 times a second at about 4 ms a copy. Now each new frame is copied once.
    const loopLimits = { seconds: 10, bytes: 32 << 20, start: 0.02, step: 250 };
    const videoStates = new WeakMap(), watchedVideos = new WeakMap();
    const fileTimelines = new Map();
    // Kept frames do not play while the session is paused, whether or not PIXI's shared ticker goes on updating them.
    let sessionPaused = false, resumedAt = 0;
    function smoothVideoLoops(pixi) {
        const VideoResource = pixi.resources && pixi.resources.VideoResource;
        if (!VideoResource || !pixi.BaseTexture || !('requestVideoFrameCallback' in HTMLVideoElement.prototype)) return;
        const update = pixi.BaseTexture.prototype.update;
        pixi.BaseTexture.prototype.update = function () {
            const resource = this.resource;
            if (this.valid && resource instanceof VideoResource && resource.source) {
                // A resize must reach the texture even when the frame has not changed.
                const frame = videoFrame(resource) + '/' + this.realWidth + 'x' + this.realHeight;
                if (frame === this.vndegenFrame) return;
                this.vndegenFrame = frame;
            }
            return update.apply(this, arguments);
        };
        const upload = VideoResource.prototype.upload;
        VideoResource.prototype.upload = function (renderer, baseTexture, glTexture, source) {
            const state = videoStates.get(this);
            const kept = !state ? null : state.mode === 'kept' ? state.frames[state.shown].canvas
                : state.mode === 'decoding' ? blankFrame(state) : null;
            return upload.call(this, renderer, baseTexture, glTexture, source || kept);
        };
        const dispose = VideoResource.prototype.dispose;
        VideoResource.prototype.dispose = function () {
            const state = videoStates.get(this);
            if (state) dropFrames(state, 'done');
            videoStates.delete(this);
            return dispose.apply(this, arguments);
        };
        // The game seeks through currentTime. WebKit's own loop seek does not, and cannot be told apart in its events.
        const currentTime = Object.getOwnPropertyDescriptor(HTMLMediaElement.prototype, 'currentTime');
        if (!currentTime || !currentTime.set) return;
        Object.defineProperty(HTMLMediaElement.prototype, 'currentTime', {
            ...currentTime,
            set(value) {
                currentTime.set.call(this, value);
                const state = watchedVideos.get(this);
                if (state) seekTo(state, Number(value));
            },
        });
    }

    // What the texture should show now: a count of the frames the video has presented, or which kept frame is due.
    function videoFrame(resource) {
        const video = resource.source;
        let state = videoStates.get(resource);
        if (!state || state.video !== video) {
            state = watchVideo(video);
            state.resource = resource;
            videoStates.set(resource, state);
        }
        if (state.mode === 'decoding') return 'decoding';
        if (state.mode !== 'kept') return state.presented;
        if (!video.loop && !state.alpha) {
            dropFrames(state, 'native');
            return state.presented;
        }
        // The kept frames play as far as the video has played since the last update. A long gap between updates counts
        // as one short step, so the clip goes on near where it stopped.
        const now = performance.now();
        const elapsed = Math.min(now - Math.max(state.tick, resumedAt), loopLimits.step);
        state.tick = now;
        if (!video.paused && !sessionPaused) {
            const position = state.position + elapsed / 1000 * video.playbackRate;
            state.position = video.loop ? position % state.length : Math.min(position, state.length);
        }
        const frames = state.frames;
        let shown = 0;
        while (shown + 1 < frames.length && frames[shown + 1].time <= state.position) shown++;
        state.shown = shown;
        return -1 - shown;
    }

    function watchVideo(video) {
        const state = { video, mode: 'keeping', presented: 0, frames: [], bytes: 0, count: -1, last: 0, position: 0 };
        const onFrame = (now, meta) => {
            if (state.mode === 'done') return;
            state.presented++;
            video.requestVideoFrameCallback(onFrame);
            if (state.mode !== 'keeping') return;
            try {
                keepFrame(state, now, meta);
            } catch (e) {
                dropFrames(state, 'native');
            }
        };
        video.requestVideoFrameCallback(onFrame);
        // A clip with alpha that plays once shows its last frame when the video ends, which may come before the
        // kept frames get there, and PIXI stops updating the texture of a video that has ended.
        video.addEventListener('ended', () => {
            if (state.mode !== 'kept' || !state.alpha || video.loop) return;
            state.position = state.length;
            redraw(state);
        });
        // A new source starts over.
        video.addEventListener('emptied', () => {
            if (state.mode === 'done') return;
            dropFrames(state, 'keeping');
            Object.assign(state, {
                count: -1, last: 0, position: 0, length: undefined, times: undefined, alpha: false,
            });
        });
        watchedVideos.set(video, state);
        return state;
    }

    // The game's seek moves the kept frames' position, also while their length is still being read, or starts keeping
    // over.
    function seekTo(state, time) {
        if (state.mode === 'keeping') dropFrames(state, 'keeping');
        else if (state.mode !== 'done') state.position = time;
    }

    // Keeps each frame of one whole pass, from a frame at the start up to the next return to it.
    function keepFrame(state, now, meta) {
        const video = state.video, time = meta.mediaTime;
        if (!(video.duration <= loopLimits.seconds)) return dropFrames(state, 'native');
        // The file is read before a clip that does not loop drops to WebKit, in case it has alpha.
        if (state.length === undefined) {
            state.length = null;
            const url = video.currentSrc;
            fileTimeline(url).then((timeline) => {
                if (video.currentSrc !== url || state.mode === 'done') return;
                if (timeline && timeline.alpha) return keepAlphaFrames(state, timeline, url);
                state.length = timeline ? timeline.length : 0;
                state.times = timeline && timeline.times;
                if (state.mode === 'ready') {
                    finishKeeping(state, performance.now());
                } else if (state.mode === 'keeping' && keptAll(state)) {
                    state.position = state.last;
                    finishKeeping(state, performance.now());
                }
            });
        }
        if (!video.loop) return dropFrames(state, 'native');
        // A pass ends where the time goes back to the start. A clip at 60 fps has more than one frame near the start.
        const atStart = time < loopLimits.start, wrapped = atStart && time < state.last;
        state.last = time;
        // A frame skipped while keeping would be missing from every later pass.
        const skipped = state.count >= 0 && meta.presentedFrames !== state.count + 1;
        state.count = meta.presentedFrames;
        if (skipped) dropFrames(state, 'keeping');
        if (wrapped && state.frames.length) {
            state.position = time;
            return finishKeeping(state, now);
        }
        if (!atStart && !state.frames.length) return;
        const canvas = document.createElement('canvas');
        canvas.width = video.videoWidth;
        canvas.height = video.videoHeight;
        state.bytes += canvas.width * canvas.height * 4;
        if (state.bytes > loopLimits.bytes) return dropFrames(state, 'native');
        canvas.getContext('2d').drawImage(video, 0, 0);
        state.frames.push({ time, canvas });
        // With the file's frame times, the pass is whole at its last frame, before WebKit stalls at its end.
        if (keptAll(state)) {
            state.position = time;
            finishKeeping(state, now);
        }
    }

    function keptAll(state) {
        return !!state.times && state.times.every((start) => state.frames.some((frame) => Math.abs(frame.time - start) < 0.002));
    }

    // Plays the kept frames once the file's length is known, null while it is being read. A video whose length cannot
    // be read, 0, plays as WebKit plays it.
    function finishKeeping(state, now) {
        const frames = state.frames;
        if (state.length === null) {
            state.mode = 'ready';
        } else if (state.length > frames[frames.length - 1].time) {
            // A clip that plays once stays at its end once there.
            const position = state.alpha && !state.video.loop ? Math.min(state.position, state.length)
                : state.position % state.length;
            Object.assign(state, { mode: 'kept', position, tick: now, shown: 0 });
        } else {
            dropFrames(state, 'native');
        }
    }

    function dropFrames(state, mode) {
        for (const frame of state.frames) frame.canvas.width = 0;
        state.frames = [];
        state.bytes = 0;
        state.mode = mode;
        state.decoding = null;
    }

    // WebKit ignores the alpha plane of VP9 WebM and shows the color the encoder left in transparent areas, often
    // white (No Socks RPG's cutscenes). A short clip marked AlphaMode 1 is decoded here with WebCodecs, color and
    // alpha separately, and its frames are kept as canvases with their alpha, as a short loop's frames are. Until they
    // are ready the texture shows nothing. A clip that cannot be decoded this way plays as WebKit plays it.
    function keepAlphaFrames(state, timeline, url) {
        const video = state.video;
        dropFrames(state, 'decoding');
        // A new source or the texture's end drops this decode, and its result then goes unused.
        const decoding = state.decoding = {};
        const current = () => state.decoding === decoding && video.currentSrc === url;
        const decode = (frames) => frames && decodeWithAlpha(frames, video.videoWidth, video.videoHeight);
        fileFrames(url).then(decode).then((kept) => {
            if (!current()) {
                for (const frame of kept || []) frame.canvas.width = 0;
                return;
            }
            if (!kept) {
                dropFrames(state, 'native');
                return redraw(state);
            }
            const bytes = kept.length * video.videoWidth * video.videoHeight * 4;
            Object.assign(state, {
                frames: kept, bytes, length: timeline.length, times: timeline.times, alpha: true,
                position: video.currentTime || 0,
            });
            finishKeeping(state, performance.now());
            redraw(state);
        }, () => {
            if (!current()) return;
            dropFrames(state, 'native');
            redraw(state);
        });
    }

    // Updates a video's texture now. PIXI stops updating it once the video has ended or paused, and VideoResource's
    // own update skips calls that come sooner than its frame rate allows, so this goes through the base Resource's.
    function redraw(state) {
        const resource = state.resource, base = pixi && pixi.resources && pixi.resources.Resource;
        if (!resource || resource.destroyed) return;
        (base ? base.prototype.update : resource.update).call(resource);
    }

    function blankFrame(state) {
        const video = state.video;
        if (!state.blank || state.blank.width !== video.videoWidth || state.blank.height !== video.videoHeight) {
            state.blank = document.createElement('canvas');
            state.blank.width = video.videoWidth;
            state.blank.height = video.videoHeight;
        }
        return state.blank;
    }

    // The frames of an alpha clip as canvases, or null when there are too many or WebCodecs cannot decode them.
    async function decodeWithAlpha(frames, width, height) {
        const profile = vp9Profile(frames[0].data);
        if (typeof VideoDecoder !== 'function' || !width || !height || profile < 0 || profile > 1
            || frames.length * width * height * 4 > loopLimits.bytes || frames.some((frame) => !frame.alpha)) {
            return null;
        }
        const config = { codec: 'vp09.0' + profile + '.10.08', codedWidth: width, codedHeight: height };
        if (!(await VideoDecoder.isConfigSupported(config)).supported) return null;
        const results = await Promise.allSettled([decodeVP9(config, frames.map((frame) => frame.data)),
            decodeVP9(config, frames.map((frame) => frame.alpha))]);
        const [color, alpha] = results.map((result) => (result.status === 'fulfilled' ? result.value : []));
        const kept = [];
        try {
            if (results.some((result) => result.status !== 'fulfilled') || color.length !== frames.length
                || alpha.length !== frames.length) return null;
            for (let i = 0; i < frames.length; i++) {
                const canvas = document.createElement('canvas');
                canvas.width = width;
                canvas.height = height;
                const context = canvas.getContext('2d', { willReadFrequently: true });
                context.drawImage(color[i], 0, 0, width, height);
                const image = context.getImageData(0, 0, width, height);
                await copyLuma(alpha[i], image.data, width, height);
                context.putImageData(image, 0, 0);
                kept.push({ time: frames[i].time, canvas });
            }
            return kept;
        } catch (e) {
            for (const frame of kept) frame.canvas.width = 0;
            throw e;
        } finally {
            for (const frame of color.concat(alpha)) frame.close();
        }
    }

    // The frames decoded from chunks in order. On any failure the decoder and every frame it made are closed.
    async function decodeVP9(config, chunks) {
        const out = [];
        let failure = null;
        const decoder = new VideoDecoder({
            output: (frame) => out.push(frame),
            error: (error) => { failure = error; },
        });
        try {
            decoder.configure(config);
            chunks.forEach((data, i) => {
                decoder.decode(new EncodedVideoChunk({ type: vp9Key(data) ? 'key' : 'delta', timestamp: i, data }));
            });
            await decoder.flush();
            if (failure) throw failure;
            return out;
        } catch (error) {
            for (const frame of out) frame.close();
            throw error;
        } finally {
            if (decoder.state !== 'closed') decoder.close();
        }
    }

    // Writes an alpha frame's luma, the alpha itself, into the alpha bytes of RGBA pixels. A frame in a planar YUV
    // format is copied out, its first plane the luma at a byte a pixel. Any other frame goes through a canvas, which
    // turns limited-range luma into grey levels close to the original.
    async function copyLuma(frame, pixels, width, height) {
        if (['I420', 'I420A', 'I422', 'I444', 'NV12'].includes(frame.format)) {
            try {
                const buffer = new Uint8Array(frame.allocationSize());
                const [luma] = await frame.copyTo(buffer);
                for (let y = 0; y < height; y++) {
                    const row = luma.offset + y * luma.stride;
                    for (let x = 0, at = y * width * 4 + 3; x < width; x++, at += 4) pixels[at] = buffer[row + x];
                }
                return;
            } catch (e) { /* through a canvas */ }
        }
        const canvas = document.createElement('canvas');
        canvas.width = width;
        canvas.height = height;
        const context = canvas.getContext('2d', { willReadFrequently: true });
        context.drawImage(frame, 0, 0, width, height);
        const grey = context.getImageData(0, 0, width, height).data;
        for (let at = 3; at < pixels.length; at += 4) pixels[at] = grey[at - 3];
        canvas.width = 0;
    }

    // A VP9 frame header starts with its marker (2), profile (two bits, low first), and for profiles 0 to 2 then
    // show_existing_frame and frame_type, which is 0 for a key frame.
    function vp9Profile(data) {
        if (!data || !data.length || data[0] >> 6 !== 2) return -1;
        return ((data[0] >> 5) & 1) | (((data[0] >> 4) & 1) << 1);
    }

    function vp9Key(data) {
        return vp9Profile(data) >= 0 && !((data[0] >> 3) & 1) && !((data[0] >> 2) & 1);
    }

    // The length a WebM file states in its Segment Info, in seconds, when each of its video frames starts, and whether
    // it has alpha, or null. Only this is kept per file.
    function fileTimeline(url) {
        if (!fileTimelines.has(url)) {
            fileTimelines.set(url, readWebM(url).then((timeline) => timeline && {
                length: timeline.length, times: timeline.times, alpha: !!timeline.alpha,
            }));
        }
        return fileTimelines.get(url);
    }

    // An alpha clip's frames with their color and alpha data, read again for each decode rather than kept.
    function fileFrames(url) {
        return readWebM(url).then((timeline) => timeline && timeline.alpha);
    }

    function readWebM(url) {
        return fetch(url).then((response) => response.arrayBuffer()).then(webmTimeline).catch(() => null);
    }

    function webmTimeline(buffer) {
        const data = new DataView(buffer);
        let at = 0;
        // An EBML variable-length integer. IDs keep their length marker, sizes drop it. A size of all ones is unknown.
        const read = (isID) => {
            const first = data.getUint8(at);
            let length = 1;
            while (length <= 8 && !(first & (0x80 >> (length - 1)))) length++;
            if (length > 8) throw new RangeError('bad EBML length');
            let value = isID ? first : first & (0xff >> length), ones = value === 0xff >> length;
            for (let i = 1; i < length; i++) {
                const byte = data.getUint8(at + i);
                value = value * 256 + byte;
                ones = ones && byte === 0xff;
            }
            at += length;
            return isID || !ones ? value : Infinity;
        };
        const number = (body, size) => {
            let value = 0;
            for (let i = 0; i < size; i++) value = value * 256 + data.getUint8(body + i);
            return value;
        };
        // Calls visit with each child of the element whose body runs from body to end, and returns after them.
        const children = (body, end, visit) => {
            for (at = body; at < end;) {
                const id = read(true), size = read(false), start = at;
                if (size === Infinity || start + size > end) throw new RangeError('bad child size');
                visit(id, start, size);
                at = start + size;
            }
        };
        // A SimpleBlock or Block starts with its track, its time after the Cluster's, and flags. A laced block holds
        // more than one frame under that one time.
        const block = (body, size) => {
            at = body;
            const track = read(false);
            const laced = data.getUint8(at + 2) & 0x06;
            if ((videoTrack === null || track === videoTrack) && laced) throw new RangeError('laced');
            const frame = new Uint8Array(buffer, at + 3, body + size - at - 3);
            return { track, time: cluster + data.getInt16(at), data: frame };
        };
        // Segment, Info, Tracks, a TrackEntry and its Video, and a Cluster hold what is read here. A BlockGroup is read
        // whole. Everything else is skipped by its size.
        const containers = [0x18538067, 0x1549a966, 0x1654ae6b, 0xae, 0xe0, 0x1f43b675];
        let scale = 1e6, duration = null, entry = {}, videoTrack = null, cluster = 0, blocks = [];
        let alpha = false;
        try {
            while (at < data.byteLength) {
                const id = read(true), size = read(false), body = at;
                if (size !== Infinity && body + size > data.byteLength) throw new RangeError('cut short');
                if (containers.includes(id)) {
                    if (id === 0xae) entry = {};
                    continue;
                }
                if (size === Infinity) break;
                if (id === 0x2ad7b1) {
                    scale = number(body, size);
                } else if (id === 0x4489) {
                    duration = size === 4 ? data.getFloat32(body) : size === 8 ? data.getFloat64(body) : null;
                } else if (id === 0xd7 || id === 0x83) {
                    entry[id === 0xd7 ? 'number' : 'type'] = number(body, size);
                    if (entry.type === 1 && entry.number !== undefined && videoTrack === null) {
                        videoTrack = entry.number;
                        alpha = !!entry.alpha;
                    }
                } else if (id === 0x53c0) {
                    // AlphaMode can come before the track's number and type.
                    entry.alpha = number(body, size) === 1;
                    if (videoTrack !== null && entry.number === videoTrack) alpha = entry.alpha;
                } else if (id === 0xe7) {
                    cluster = number(body, size);
                } else if (id === 0xa3) {
                    blocks.push(block(body, size));
                } else if (id === 0xa0) {
                    // A BlockGroup's Block and BlockAdditions come in any order. The alpha plane of the Block's
                    // frame, coded as its own frame, is the BlockAdditional of the BlockMore whose BlockAddID is 1,
                    // the default.
                    let found = null, alphaData;
                    children(body, body + size, (child, childBody, childSize) => {
                        if (child === 0xa1) found = block(childBody, childSize);
                        if (child !== 0x75a1) return;
                        children(childBody, childBody + childSize, (more, moreBody, moreSize) => {
                            if (more !== 0xa6) return;
                            let addID = 1, additional;
                            children(moreBody, moreBody + moreSize, (field, fieldBody, fieldSize) => {
                                if (field === 0xee) addID = number(fieldBody, fieldSize);
                                if (field === 0xa5) additional = new Uint8Array(buffer, fieldBody, fieldSize);
                            });
                            if (addID === 1 && additional) alphaData = additional;
                        });
                    });
                    if (found) blocks.push({ ...found, alpha: alphaData });
                }
                at = body + size;
            }
        } catch (e) {
            // A frame list that may be short would end a pass early, so a file that does not read cleanly gives none.
            blocks = [];
        }
        if (!(duration > 0 && scale > 0)) return null;
        const times = [...new Set(blocks.filter((block) => videoTrack === null || block.track === videoTrack)
            .map((block) => block.time * scale / 1e9))].sort((a, b) => a - b);
        const frames = blocks.filter((block) => videoTrack === null || block.track === videoTrack)
            .map((block) => ({ time: block.time * scale / 1e9, data: block.data, alpha: block.alpha }));
        // The frames' data is kept only for a clip with alpha, which is decoded from it.
        return {
            length: duration * scale / 1e9,
            times: times.length ? times : null,
            alpha: alpha && frames.length ? frames : null,
        };
    }
    let pixi;
    Object.defineProperty(window, 'PIXI', {
        configurable: true,
        get: () => pixi,
        set(value) {
            if (value === pixi) return;
            pixi = value;
            if (value) smoothVideoLoops(value);
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

    // A canvas keeps whatever font its text was drawn in, so text drawn before a font substitute loads would stay in
    // the fallback font. The boot scene waits for the substitutes the game names (see fonts.js).
    document.addEventListener('load', () => {
        if (typeof Scene_Boot !== 'function') return;
        wrap(Scene_Boot.prototype, 'isReady', (isReady) => function () {
            return window.__vndegen.fontsReady !== false && isReady.apply(this, arguments);
        });
    }, true);

    // iOS can end WebKit's GPU process, such as when it needs the memory, and the page loses its WebGL context. WebKit
    // restores it at once and PIXI uploads its textures again, but two parts of MZ kept what lived in the lost context.
    // Tile layers copy their tilesets into the tilemap renderer's own textures only when the tileset changes, so the
    // map went black. Effekseer, which draws battle animations, went on drawing with its lost buffers and programs, and
    // its failing calls broke PIXI's next buffer uploads, so sprites drew in the wrong places and order, more of them
    // with each battle, until the map showed none. After a restore, each tile layer copies its tilesets again, and
    // Effekseer gets a new context whose effects load again. Animations that were playing end, as their effect is gone.
    let contextGeneration = 0;
    window.addEventListener('webglcontextrestored', (event) => {
        const graphics = window.Graphics;
        if (!graphics || !graphics._app || event.target !== graphics._app.view) return;
        contextGeneration++;
        console.warn('WebGL context restored, restoring the tilemap and Effekseer');
        if (typeof graphics._createEffekseerContext !== 'function' || !window.effekseer) return;
        retireEffekseer(graphics);
        // MZ drops the whole PIXI application when Effekseer fails to start. Here the game keeps drawing, without
        // battle animations until the next restore.
        const app = graphics._app;
        try {
            graphics._createEffekseerContext();
        } finally {
            if (graphics._app !== app) {
                graphics._app = app;
                graphics._effekseer = null;
            }
        }
    }, true);

    // Frees the lost Effekseer context and every effect it made, including ones a plugin loaded itself and ones still
    // waiting for a file or texture. Their callbacks do nothing from now on, so nothing that arrives later reaches the
    // released context or reports an error for it. Deleting GL objects that went with the lost context only sets error
    // flags, which are cleared so nothing reads them as a later call's.
    function retireEffekseer(graphics) {
        const lost = graphics._effekseer, gl = graphics._app.renderer.gl;
        const effects = new Set(lost && lost.vndegenEffects);
        if (window.EffectManager) {
            for (const effect of Object.values(EffectManager._cache)) if (effect) effects.add(effect);
            EffectManager._cache = {};
        }
        if (lost) lost.vndegenRetired = true;
        for (const effect of effects) {
            effect._load = effect._update = effect._reload = () => {};
            effect.onload = effect.onerror = null;
        }
        if (!lost) return;
        for (const effect of effects) {
            if (effect.context !== lost || !effect.nativeptr) continue;
            // releaseEffect only warns about an effect still waiting for its textures, which holds native memory too.
            effect.isLoaded = true;
            try { lost.releaseEffect(effect); } catch (e) { /* already gone */ }
        }
        try { effekseer.releaseContext(lost); } catch (e) { /* already gone */ }
        for (let i = 0; i < 16 && gl.getError() !== gl.NO_ERROR; i++);
    }

    // Each Effekseer context keeps the effects it loads, for retireEffekseer. Their load and error callbacks are
    // wrapped as they are passed in, as Effekseer hands them on to its loaders by value.
    function trackEffects(context) {
        const effects = context.vndegenEffects = new Set();
        const loadEffect = context.loadEffect, releaseEffect = context.releaseEffect;
        if (typeof loadEffect !== 'function' || typeof releaseEffect !== 'function') return;
        context.loadEffect = function (...args) {
            const live = args.map((arg) => typeof arg !== 'function' ? arg
                : function () { if (!context.vndegenRetired) return arg.apply(this, arguments); });
            const effect = loadEffect.apply(this, live);
            if (effect) effects.add(effect);
            return effect;
        };
        context.releaseEffect = function (effect) {
            effects.delete(effect);
            return releaseEffect.apply(this, arguments);
        };
    }

    document.addEventListener('load', () => {
        const layer = window.Tilemap && Tilemap.Layer && Tilemap.Layer.prototype;
        if (layer) {
            wrap(layer, 'render', (render) => function () {
                if (this.vndegenContext !== contextGeneration) {
                    if (this.vndegenContext !== undefined) this._needsTexturesUpdate = true;
                    this.vndegenContext = contextGeneration;
                }
                return render.apply(this, arguments);
            });
        }
        if (window.effekseer) {
            wrap(effekseer, 'createContext', (createContext) => function () {
                const context = createContext.apply(this, arguments);
                if (context) trackEffects(context);
                return context;
            });
        }
        // MV's animations are sprites, which PIXI restores.
        const animation = window.Graphics && Graphics._createEffekseerContext && typeof Sprite_Animation === 'function'
            && Sprite_Animation.prototype;
        if (!animation) return;
        // Every method that reads an animation's handle first drops a handle, or an effect not yet playing, that
        // belongs to an Effekseer context other than the current one.
        for (const name of ['update', '_render', 'destroy', 'setRotation', 'updateEffectGeometry', 'checkEnd']) {
            wrap(animation, name, (method) => function () {
                forgetLostEffect(this);
                return method.apply(this, arguments);
            });
        }
    }, true);
    function forgetLostEffect(animation) {
        const current = window.Graphics && Graphics._effekseer;
        const handle = animation._handle;
        if (handle && handle.context && handle.context !== current) animation._handle = null;
        const effect = animation._effect;
        if (!animation._started && effect && effect.context && effect.context !== current && animation._animation
            && window.EffectManager) {
            animation._effect = EffectManager.load(animation._animation.effectName);
        }
    }

    // Early MV cores (1.2.0 in the library) build the save folder from the page's address: index.html's folder, less a
    // last /www, plus save/. On the PC that is the game's top-level save/. Here the address is vndegen://game/index.html,
    // so the folder came out as /save/, outside the game folder, and every save failed. The same rule applied to
    // index.html's path in the game folder gives the PC's folder. GameFolder.saveDirectory follows it too. The method is
    // checked after every script loads, since a plugin may replace it.
    document.addEventListener('load', () => {
        const storage = window.StorageManager;
        if (!storage || typeof storage.localFileDirectoryPath !== 'function'
            || !String(storage.localFileDirectoryPath).includes('location.pathname')) return;
        storage.localFileDirectoryPath = function () {
            return window.process.mainModule.filename.replace(/(\/www|)\/[^\/]*$/, '/save/');
        };
    }, true);

    // A paused session stops the engine's scene loop. On resume, Input.clear drops keys that were down when it paused.
    window.__vndegen.onPause = (paused) => {
        sessionPaused = paused;
        if (!paused) resumedAt = performance.now();
        const scenes = window.SceneManager;
        if (paused) {
            if (scenes && scenes.stop) scenes.stop();
        } else {
            if (window.Input && window.Input.clear) window.Input.clear();
            if (scenes && scenes.resume) scenes.resume();
        }
    };
})();
