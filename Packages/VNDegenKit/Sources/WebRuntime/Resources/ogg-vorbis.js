// Ogg Vorbis for pages that play .ogg files, which WebKit on iOS cannot decode: <audio> fails with "not supported" and
// decodeAudioData rejects the bytes. libvorbis, compiled to WebAssembly (vorbis.wasm), decodes them here in the page, so
// a malformed file is handled inside WebKit's sandboxed content process and never reaches the app.
// decodeAudioData gets the decoded samples. A media element gets a 16-bit WAV of the file, kept in a bounded cache.
// Injected at document start, after the NW.js shim. config holds the WebAssembly module and the limits.
(function () {
    'use strict';
    const config = __VNDEGEN_OGG__;

    // Decodes the Vorbis stream in an Ogg file, and the streams chained after it, with libvorbis compiled to WebAssembly.
    // Each call gets its own instance, so nothing a malformed file does to the decoder's memory outlives the call. Returns
    // { sampleRate, channels } with one Float32Array per channel, or { sampleRate, wav } with a 16-bit WAV file when `wav`
    // is set. Throws for anything that is not Ogg Vorbis, for a chain whose streams differ in format, and before the
    // output would pass `maxSamples` sample values.
    function decodeVorbis(module, bytes, maxSamples, wav) {
        const fail = (why) => { throw new Error('Ogg Vorbis: ' + why); };
        const x = new WebAssembly.Instance(module, { a: {
            a: Math.cos, b: Math.atan, c: Math.sin, d: () => {}, e: () => fail('decoder aborted'), f: () => 0,
            g: Math.pow, h: Math.log, i: Math.exp, j: () => fail('decoder exited'), k: () => false,
        } }).exports;
        x.m();
        const maxPacket = 1 << 20;
        const input = x.o(maxPacket), fields = x.o(24), errors = x.o(8 * 128);
        if (!input || !fields || !errors) fail('out of decoder memory');
        const [inputLength, output, channelsAt, rateAt, countAt, errorCount] = [0, 4, 8, 12, 16, 20].map((offset) => fields + offset);
        const heap = () => new DataView(x.l.buffer);
        const failed = () => heap().getInt32(errorCount, true) > 0;

        // Ogg pages (RFC 3533): "OggS", version, flags, granule position, serial, sequence, CRC, segment count, lacing values.
        // Yields the pages of the stream being decoded, one page at a time. That is the first page's stream and, once it
        // ends, the stream of the next page that begins one, since a chained file (RFC 3533 section 4) follows one stream
        // with the next. `first` marks the first page of each. A cut-off file ends at its last whole page, and bytes after
        // the last stream that are not a page, such as a tag, are ignored.
        const data = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
        function* pages() {
            let serial = -1, ended = false;
            for (let at = 0; at + 27 <= bytes.length;) {
                if (data.getUint32(at, false) !== 0x4f676753) {
                    if (ended) return;
                    fail('lost page sync');
                }
                const segments = bytes[at + 26], flags = bytes[at + 5], pageSerial = data.getUint32(at + 14, true);
                let end = at + 27 + segments;
                if (end > bytes.length) return;
                for (let i = 0; i < segments; i++) end += bytes[at + 27 + i];
                if (end > bytes.length) return;
                const first = serial < 0 || (ended && (flags & 2) !== 0);
                if (first) {
                    serial = pageSerial;
                    ended = false;
                }
                if (!ended && pageSerial === serial) {
                    ended = (flags & 4) !== 0;
                    yield { first, at, segments, granule: data.getUint32(at + 6, true) + data.getInt32(at + 10, true) * 4294967296 };
                }
                at = end;
            }
        }

        // Each stream's last granule position is its length in frames. A file can lie about it, so the output grows if
        // decoding produces more, and decoding can also run a little past it, which is cut off.
        let expected = 0, granule = 0;
        for (const page of pages()) {
            if (page.first) expected += Math.max(granule, 0);
            granule = page.granule;
        }
        expected += Math.max(granule, 0);

        let decoder = 0, packets = 0, channels = 0, rate = 0, frames = 0, start = 0, store = null, capacity = 0;
        const allocate = (size) => {
            if (size * channels > maxSamples) fail('longer than the limit');
            const grown = wav ? new Int16Array(size * channels) : Array.from({ length: channels }, () => new Float32Array(size));
            if (store) {
                if (wav) grown.set(store);
                else grown.forEach((channel, c) => channel.set(store[c]));
            }
            store = grown;
            capacity = size;
        };
        const finish = () => {
            if (!decoder) return;
            x.s(decoder);
            decoder = 0;
            if (granule > 0 && start + granule < frames) frames = start + granule;
        };
        // Decodes the packet of `size` bytes in the decoder's input buffer.
        const take = (size) => {
            const type = new Uint8Array(x.l.buffer, input, 1)[0];
            heap().setInt32(inputLength, size, true);
            heap().setInt32(errorCount, 0, true);
            if (packets === 0) {
                if (type !== 1) fail('not a Vorbis stream');
                x.p(decoder, 1);
                if (failed()) fail('bad identification header');
            } else if (packets === 2) {
                if (type !== 5) fail('missing setup header');
                x.p(decoder, 0);
                if (failed()) fail('bad setup header');
                x.q(decoder);
                if (failed()) fail('could not start decoding');
                const c = heap().getInt32(channelsAt, true), r = heap().getInt32(rateAt, true);
                if (c < 1 || c > 32 || r < 3000 || r > 384000) fail('unsupported format');
                if (store && (c !== channels || r !== rate)) fail('chained streams differ in format');
                channels = c;
                rate = r;
                if (!store) allocate(expected > 0 && expected * channels <= maxSamples ? expected : 4096);
            } else if (packets > 2) {
                x.r(decoder);
                const count = heap().getInt32(countAt, true);
                if (count > 0) {
                    if (frames + count > capacity) allocate(Math.max(frames + count, Math.min(capacity * 2, Math.floor(maxSamples / channels))));
                    const pointers = heap().getUint32(output, true);
                    for (let c = 0; c < channels; c++) {
                        const decoded = new Float32Array(x.l.buffer, heap().getUint32(pointers + 4 * c, true), count);
                        if (!wav) {
                            store[c].set(decoded, frames);
                            continue;
                        }
                        for (let i = 0, at = frames * channels + c; i < count; i++, at += channels) {
                            const s = decoded[i];
                            store[at] = s >= 1 ? 32767 : s <= -1 ? -32768 : s < 0 ? s * 32768 : s * 32767;
                        }
                    }
                    frames += count;
                }
            }
            packets++;
        };

        // Packets are put together in the decoder's input buffer as their segments arrive, so a packet over the limit
        // fails as soon as it passes it.
        let size = 0;
        for (const page of pages()) {
            if (page.first) {
                finish();
                decoder = x.n(input, inputLength, output, channelsAt, rateAt, countAt, errors, errorCount, 128);
                if (!decoder) fail('out of decoder memory');
                packets = 0;
                size = 0;
                start = frames;
            }
            granule = page.granule;
            for (let i = 0, body = page.at + 27 + page.segments; i < page.segments; i++) {
                const length = bytes[page.at + 27 + i];
                if (size + length > maxPacket) fail('packet too large');
                new Uint8Array(x.l.buffer, input + size, length).set(bytes.subarray(body, body + length));
                body += length;
                size += length;
                if (length === 255) continue;
                // An empty packet carries nothing for Vorbis.
                if (size) take(size);
                size = 0;
            }
        }
        finish();
        if (!frames) fail('no audio');
        if (!wav) return { sampleRate: rate, channels: store.map((channel) => channel.slice(0, frames)) };

        const file = new DataView(new ArrayBuffer(44 + frames * channels * 2));
        const tag = (offset, text) => { for (let i = 0; i < 4; i++) file.setUint8(offset + i, text.charCodeAt(i)); };
        tag(0, 'RIFF'); file.setUint32(4, 36 + frames * channels * 2, true); tag(8, 'WAVE');
        tag(12, 'fmt '); file.setUint32(16, 16, true); file.setUint16(20, 1, true); file.setUint16(22, channels, true);
        file.setUint32(24, rate, true); file.setUint32(28, rate * channels * 2, true); file.setUint16(32, channels * 2, true);
        file.setUint16(34, 16, true); tag(36, 'data'); file.setUint32(40, frames * channels * 2, true);
        new Int16Array(file.buffer, 44).set(store.subarray(0, frames * channels));
        return { sampleRate: rate, wav: file.buffer };
    }

    const wasm = WebAssembly.compile(Uint8Array.from(atob(config.wasm), (c) => c.charCodeAt(0)));
    const isOggBytes = (b) => b.length >= 4 && b[0] === 0x4f && b[1] === 0x67 && b[2] === 0x67 && b[3] === 0x53;
    const absolute = (url) => { try { return new URL(String(url), document.baseURI).href; } catch (e) { return null; } };
    const isOggURL = (url) => /\.og[ga]$/i.test(new URL(url).pathname);
    const aborted = (message) => new DOMException(message, 'AbortError');

    // Decoding runs in at most config.limits.workers workers, so a long track never stalls the game. A job nobody waits
    // for any more leaves the queue, or stops its worker if it already started.
    const workerSource = decodeVorbis.toString() + `
        let module;
        onmessage = ({ data }) => {
            if (data.module) { module = data.module; return; }
            let result;
            try {
                result = decodeVorbis(module, new Uint8Array(data.bytes), data.maxSamples, data.wav);
            } catch (e) {
                postMessage({ error: String((e && e.message) || e) });
                return;
            }
            postMessage(result, result.wav ? [result.wav] : result.channels.map((c) => c.buffer));
        };`;
    const workerURL = URL.createObjectURL(new Blob([workerSource], { type: 'text/javascript' }));
    const queue = [], idle = [];
    let running = 0;

    // `load` gets the job's abort signal and returns the file's bytes. It runs when the job leaves the queue, so a queued
    // job holds nothing of its own.
    function decode(load, wav) {
        const job = { load, wav, controller: new AbortController(), done: false };
        job.promise = new Promise((resolve, reject) => { job.resolve = resolve; job.reject = reject; });
        queue.push(job);
        pump();
        return job;
    }

    function pump() {
        while (queue.length && running < config.limits.workers) {
            const job = queue.shift();
            running++;
            run(job).finally(() => { running--; job.done = true; pump(); });
        }
    }

    async function run(job) {
        const signal = job.controller.signal;
        try {
            const bytes = await job.load(signal);
            const module = await wasm;
            signal.throwIfAborted();
            let worker = idle.pop();
            if (!worker) {
                try {
                    worker = new Worker(workerURL);
                } catch (e) {
                    // No workers here: decode on the page's own thread.
                    return job.resolve(decodeVorbis(module, new Uint8Array(bytes), config.limits.maxSamples, job.wav));
                }
                worker.postMessage({ module });
            }
            let stop;
            const result = await new Promise((resolve, reject) => {
                stop = () => { worker.terminate(); worker = null; reject(signal.reason); };
                signal.addEventListener('abort', stop);
                worker.onerror = (event) => { worker.terminate(); worker = null; reject(new Error(event.message || 'decoder failed')); };
                worker.onmessage = ({ data }) => (data.error ? reject(new Error(data.error)) : resolve(data));
                worker.postMessage({ bytes, maxSamples: config.limits.maxSamples, wav: job.wav }, [bytes]);
            }).finally(() => {
                signal.removeEventListener('abort', stop);
                if (worker) idle.push(worker);
            });
            job.resolve(result);
        } catch (e) {
            job.reject(e);
        }
    }

    // Stops a job nobody waits for any more. It leaves the queue, or its fetch or its worker stops and frees the slot.
    function cancel(job) {
        if (job.done) return;
        const index = queue.indexOf(job);
        if (index >= 0) {
            queue.splice(index, 1);
            job.done = true;
            job.reject(aborted('nobody is waiting for this audio'));
        } else {
            job.controller.abort(aborted('nobody is waiting for this audio'));
        }
    }

    function hurry(job) {
        const index = queue.indexOf(job);
        if (index > 0) queue.unshift(queue.splice(index, 1)[0]);
    }

    async function fetchOgg(url, signal) {
        const response = await fetch(url, { signal });
        if (!response.ok) throw new Error('HTTP ' + response.status);
        if (Number(response.headers.get('Content-Length')) > config.limits.maxInputBytes) throw new Error('file too large');
        const bytes = await response.arrayBuffer();
        if (bytes.byteLength > config.limits.maxInputBytes || !isOggBytes(new Uint8Array(bytes, 0, Math.min(4, bytes.byteLength)))) {
            throw new Error('not an Ogg file');
        }
        return bytes;
    }

    // Web Audio: Tyrano 5's Howler and similar engines fetch the file and decode it here.
    const Context = window.BaseAudioContext || window.AudioContext || window.webkitAudioContext;
    if (Context) {
        const nativeDecode = Context.prototype.decodeAudioData;
        Context.prototype.decodeAudioData = function (buffer, success, failure) {
            const view = buffer instanceof ArrayBuffer ? new Uint8Array(buffer) : null;
            if (!view || !isOggBytes(view) || view.length > config.limits.maxInputBytes) return nativeDecode.apply(this, arguments);
            // decodeAudioData owns the buffer from the call on and detaches the page's copy, as Chromium does, so a page
            // that reuses the buffer can't change a queued decode. The worker gets a copy only once the job starts, so
            // queued requests hold no copies.
            const bytes = new Uint8Array(buffer.transfer());
            const promise = decode(() => bytes.slice().buffer, false).promise.then((result) => {
                const out = this.createBuffer(result.channels.length, result.channels[0].length, result.sampleRate);
                result.channels.forEach((channel, c) => out.copyToChannel(channel, c));
                return out;
            }, (error) => {
                console.warn('Ogg decode failed, trying WebKit: ' + error.message);
                return nativeDecode.call(this, bytes.slice().buffer);
            });
            if (success || failure) promise.then(success, failure);
            return promise;
        };
    }

    // Media elements: each .ogg URL is decoded once to a WAV blob. Blobs no element uses go, oldest first, once the
    // cache holds more than config.limits.maxCacheBytes. A decode no element waits for stops, and its entry goes at once,
    // so asking for the URL again starts a new decode rather than joining the cancelled one.
    const tracks = new Map();
    let cachedBytes = 0, uses = 0;

    function track(url) {
        let entry = tracks.get(url);
        if (!entry) {
            entry = { url, users: 0, size: 0, blobURL: null, used: 0, job: decode((signal) => fetchOgg(url, signal), true) };
            entry.promise = entry.job.promise.then((result) => {
                if (tracks.get(url) !== entry) throw aborted('nobody is waiting for this audio');
                entry.blobURL = URL.createObjectURL(new Blob([result.wav], { type: 'audio/wav' }));
                entry.size = result.wav.byteLength;
                cachedBytes += entry.size;
                trim();
                return entry.blobURL;
            }, (error) => {
                forget(entry);
                throw error;
            });
            tracks.set(url, entry);
        }
        entry.users++;
        entry.used = ++uses;
        trim();
        return entry;
    }

    function forget(entry) {
        if (tracks.get(entry.url) === entry) tracks.delete(entry.url);
    }

    function trim() {
        while (cachedBytes > config.limits.maxCacheBytes) {
            let oldest = null;
            for (const entry of tracks.values()) {
                if (entry.blobURL && entry.users === 0 && (!oldest || entry.used < oldest.used)) oldest = entry;
            }
            if (!oldest) return;
            forget(oldest);
            URL.revokeObjectURL(oldest.blobURL);
            cachedBytes -= oldest.size;
        }
    }

    const proto = HTMLMediaElement.prototype;
    const src = Object.getOwnPropertyDescriptor(proto, 'src');
    const nativePlay = proto.play, nativePause = proto.pause, nativeLoad = proto.load, nativeCanPlayType = proto.canPlayType;
    const nativeSetAttribute = Element.prototype.setAttribute, nativeRemoveAttribute = Element.prototype.removeAttribute;
    // Per element: the .ogg URL it was given, its cache entry, and play() calls made before the decode finished.
    const elements = new WeakMap();
    const released = new FinalizationRegistry((state) => release(state));

    // Rejects the play() calls waiting for the decode.
    function interrupt(state, message) {
        state.wantsPlay = false;
        for (const waiter of state.waiters.splice(0)) waiter.reject(aborted(message));
    }

    function release(state) {
        const entry = state.entry;
        if (!entry) return;
        state.entry = null;
        state.url = null;
        state.generation++;
        if (state.pending) {
            state.pending = false;
            interrupt(state, 'The play() request was interrupted by a new load request.');
        }
        entry.users--;
        if (!entry.blobURL && entry.users === 0) {
            forget(entry);
            cancel(entry.job);
        }
        trim();
    }

    function assign(element, url) {
        let state = elements.get(element);
        if (!state) {
            state = { url: null, entry: null, generation: 0, pending: false, wantsPlay: false, waiters: [] };
            elements.set(element, state);
            released.register(element, state);
        }
        // Taking the new entry first keeps a decode of the same URL alive when the element asks for it again.
        const entry = track(url);
        release(state);
        // The element can get the same entry back, so only the generation tells this assignment from an earlier one.
        const generation = ++state.generation;
        state.url = url;
        state.entry = entry;
        // Stop what the element played before, as a new src would, without giving it an empty one, which is an error.
        nativeRemoveAttribute.call(element, 'src');
        nativeLoad.call(element);
        if (entry.blobURL) return src.set.call(element, entry.blobURL);
        state.pending = true;
        entry.promise.catch((error) => {
            // The original bytes are no worse than nothing, and a format this cannot read may still play.
            if (state.generation === generation) console.warn('Ogg decode failed for ' + url + ': ' + error.message);
            return url;
        }).then((source) => {
            if (state.generation !== generation) return;
            state.pending = false;
            src.set.call(element, source);
            const waiters = state.waiters.splice(0);
            if (!state.wantsPlay) return;
            state.wantsPlay = false;
            nativePlay.call(element).then((v) => waiters.forEach((w) => w.resolve(v)), (e) => waiters.forEach((w) => w.reject(e)));
        });
    }

    function setSource(element, value) {
        const url = absolute(value);
        if (url && isOggURL(url)) return assign(element, url);
        const state = elements.get(element);
        if (state) release(state);
        src.set.call(element, value);
    }

    Object.defineProperty(proto, 'src', {
        configurable: true,
        enumerable: true,
        get() {
            const state = elements.get(this);
            return state && state.url ? state.url : src.get.call(this);
        },
        set(value) {
            setSource(this, value);
        },
    });
    // The src attribute is the same source as the property. HTML attribute names are case-insensitive.
    const isSrc = (name) => String(name).toLowerCase() === 'src';
    proto.setAttribute = function (name, value) {
        if (!isSrc(name)) return nativeSetAttribute.apply(this, arguments);
        setSource(this, String(value));
    };
    proto.removeAttribute = function (name) {
        if (isSrc(name)) {
            const state = elements.get(this);
            if (state) release(state);
        }
        return nativeRemoveAttribute.apply(this, arguments);
    };
    proto.play = function () {
        const state = elements.get(this);
        if (!state || !state.pending) return nativePlay.apply(this, arguments);
        state.wantsPlay = true;
        hurry(state.entry.job);
        return new Promise((resolve, reject) => state.waiters.push({ resolve, reject }));
    };
    proto.pause = function () {
        const state = elements.get(this);
        if (state && state.pending) interrupt(state, 'The play() request was interrupted by a call to pause().');
        return nativePause.apply(this, arguments);
    };
    proto.load = function () {
        const state = elements.get(this);
        if (!state || !state.pending) return nativeLoad.apply(this, arguments);
        // The decode goes on and the source stays, but load() drops the play() calls made before it, as it does natively.
        interrupt(state, 'The play() request was interrupted by a new load request.');
    };
    proto.canPlayType = function (type) {
        return /^audio\/(ogg|vorbis)\b/i.test(String(type)) && !/opus/i.test(String(type)) ? 'probably' : nativeCanPlayType.apply(this, arguments);
    };
    // new Audio(url) sets its source without going through the src property.
    const NativeAudio = window.Audio;
    window.Audio = function Audio(url) {
        const audio = new NativeAudio();
        if (url !== undefined) audio.src = url;
        return audio;
    };
    window.Audio.prototype = NativeAudio.prototype;

    // Engine scripts injected after this one, such as the RPG Maker additions, read it to keep Ogg visible.
    window.__vndegen.decodesOgg = true;
})();
