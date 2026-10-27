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
    const videoStates = new WeakMap();
    const fileLengths = new Map();
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
            const kept = state && state.mode === 'kept' ? state.frames[state.shown].canvas : null;
            return upload.call(this, renderer, baseTexture, glTexture, source || kept);
        };
        const dispose = VideoResource.prototype.dispose;
        VideoResource.prototype.dispose = function () {
            const state = videoStates.get(this);
            if (state) dropFrames(state, 'done');
            videoStates.delete(this);
            return dispose.apply(this, arguments);
        };
    }

    // What the texture should show now: a count of the frames the video has presented, or which kept frame is due.
    function videoFrame(resource) {
        const video = resource.source;
        let state = videoStates.get(resource);
        if (!state || state.video !== video) {
            state = watchVideo(video);
            videoStates.set(resource, state);
        }
        if (state.mode !== 'kept') return state.presented;
        if (!video.loop) {
            dropFrames(state, 'native');
            return state.presented;
        }
        // The kept frames play as far as the video has played since the last update. A long gap, such as the session
        // pausing the game, counts as one short step, so the clip goes on near where it stopped.
        const now = performance.now();
        const elapsed = Math.min(now - state.tick, loopLimits.step);
        state.tick = now;
        if (!video.paused) state.position = (state.position + elapsed / 1000 * video.playbackRate) % state.length;
        const frames = state.frames;
        let shown = 0;
        while (shown + 1 < frames.length && frames[shown + 1].time <= state.position) shown++;
        state.shown = shown;
        return -1 - shown;
    }

    function watchVideo(video) {
        const state = { video, mode: 'keeping', presented: 0, frames: [], bytes: 0, count: -1 };
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
        // A new source starts over. WebKit's own loop seeks back to 0, so only a seek elsewhere is the game's: it moves
        // the kept frames' position, or starts keeping over.
        video.addEventListener('emptied', () => {
            if (state.mode === 'done') return;
            dropFrames(state, 'keeping');
            state.count = -1;
            state.length = undefined;
        });
        video.addEventListener('seeked', () => {
            if (state.mode === 'done' || !(video.currentTime > loopLimits.start)) return;
            if (state.mode === 'kept') state.position = video.currentTime % state.length;
            else if (state.mode === 'keeping') dropFrames(state, 'keeping');
        });
        return state;
    }

    // Keeps each frame of one whole pass, from a frame at the start up to the next return to it.
    function keepFrame(state, now, meta) {
        const video = state.video, time = meta.mediaTime;
        if (!video.loop || !(video.duration <= loopLimits.seconds)) return dropFrames(state, 'native');
        if (state.length === undefined) {
            state.length = null;
            const url = video.currentSrc;
            fileLength(url).then((length) => {
                if (video.currentSrc !== url || state.mode === 'done') return;
                state.length = length || 0;
                if (state.mode === 'ready') finishKeeping(state, performance.now(), 0);
            });
        }
        const atStart = time < loopLimits.start;
        // A frame skipped while keeping would be missing from every later pass.
        const skipped = state.count >= 0 && meta.presentedFrames !== state.count + 1;
        state.count = meta.presentedFrames;
        if (skipped) dropFrames(state, 'keeping');
        if (atStart && state.frames.length) return finishKeeping(state, now, time);
        if (!atStart && !state.frames.length) return;
        const canvas = document.createElement('canvas');
        canvas.width = video.videoWidth;
        canvas.height = video.videoHeight;
        state.bytes += canvas.width * canvas.height * 4;
        if (state.bytes > loopLimits.bytes) return dropFrames(state, 'native');
        canvas.getContext('2d').drawImage(video, 0, 0);
        state.frames.push({ time, canvas });
    }

    // Plays the kept frames once the file's length is known, null while it is being read. A video whose length cannot
    // be read, 0, plays as WebKit plays it.
    function finishKeeping(state, now, position) {
        const frames = state.frames;
        if (state.length === null) {
            state.mode = 'ready';
        } else if (state.length > frames[frames.length - 1].time) {
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
    }

    // The length a WebM file states in its Segment Info, in seconds, or null.
    function fileLength(url) {
        if (!fileLengths.has(url)) {
            fileLengths.set(url, fetch(url).then((response) => response.arrayBuffer()).then(webmLength).catch(() => null));
        }
        return fileLengths.get(url);
    }

    function webmLength(buffer) {
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
        // Info usually comes first, but may follow the Clusters, which are skipped by their sizes.
        let end = data.byteLength, scale = 1e6, duration = null, infoEnd = -1;
        while (at < end && at !== infoEnd) {
            const id = read(true), size = read(false), body = at;
            if (id === 0x18538067 || id === 0x1549a966) {
                // Segment, then its Info, which holds the length.
                if (size !== Infinity) end = Math.min(end, body + size);
                if (id === 0x1549a966) infoEnd = size === Infinity ? -1 : body + size;
                continue;
            }
            if (size === Infinity) break;
            if (id === 0x2ad7b1) {
                scale = 0;
                for (let i = 0; i < size; i++) scale = scale * 256 + data.getUint8(body + i);
            } else if (id === 0x4489) {
                duration = size === 4 ? data.getFloat32(body) : size === 8 ? data.getFloat64(body) : null;
            }
            at = body + size;
        }
        return duration > 0 && scale > 0 ? duration * scale / 1e9 : null;
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
