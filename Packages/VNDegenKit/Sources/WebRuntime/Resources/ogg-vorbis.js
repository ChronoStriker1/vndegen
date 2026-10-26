// Ogg Vorbis for pages that play .ogg files, which WebKit on iOS cannot decode: <audio> fails with "not supported" and
// decodeAudioData rejects the bytes. libvorbis, compiled to WebAssembly (vorbis.wasm), decodes them here in the page, so
// a malformed file is handled inside WebKit's sandboxed content process and never reaches the app.
// decodeAudioData gets the decoded samples. A media element gets a 16-bit WAV of the file, kept in a bounded cache.
// Injected at document start, after the NW.js shim. config holds the WebAssembly module and the limits.
(function () {
    'use strict';
    const config = __VNDEGEN_OGG__;

    // Decodes the first Vorbis stream in an Ogg file with libvorbis compiled to WebAssembly. Each call gets its own instance,
    // so nothing a malformed file does to the decoder's memory outlives the call. Returns { sampleRate, channels } with one
    // Float32Array per channel, or { sampleRate, wav } with a 16-bit WAV file when `wav` is set. Throws for anything that is
    // not Ogg Vorbis, and before the output would pass `maxSamples` sample values.
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
        const decoder = x.n(input, inputLength, output, channelsAt, rateAt, countAt, errors, errorCount, 128);
        if (!decoder) fail('out of decoder memory');
        const heap = () => new DataView(x.l.buffer);
        const send = (packet) => {
            new Uint8Array(x.l.buffer, input, packet.length).set(packet);
            heap().setInt32(inputLength, packet.length, true);
            heap().setInt32(errorCount, 0, true);
        };
        const failed = () => heap().getInt32(errorCount, true) > 0;

        // Ogg pages (RFC 3533): "OggS", version, flags, granule position, serial, sequence, CRC, segment count, lacing values.
        const data = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
        const pages = [];
        for (let at = 0; at + 27 <= bytes.length;) {
            if (data.getUint32(at, false) !== 0x4f676753) fail('lost page sync');
            const segments = bytes[at + 26];
            let end = at + 27 + segments;
            if (end > bytes.length) break;
            for (let i = 0; i < segments; i++) end += bytes[at + 27 + i];
            // A cut-off file plays up to its last whole page.
            if (end > bytes.length) break;
            const serial = data.getUint32(at + 14, true);
            if (!pages.length || serial === pages[0].serial) {
                pages.push({ at, serial, flags: bytes[at + 5], granule: data.getUint32(at + 6, true) + data.getInt32(at + 10, true) * 4294967296 });
                if (bytes[at + 5] & 4) break;
            }
            at = end;
        }
        // The last page's granule position is the stream's length in frames. A file can lie about it, so the output grows if
        // decoding produces more, and decoding can also run a little past it, which is cut off.
        const lastGranule = pages.length ? pages[pages.length - 1].granule : -1;

        let packets = 0, channels = 0, rate = 0, frames = 0, store = null, capacity = 0;
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
        const take = (packet) => {
            if (packet.length > maxPacket) fail('packet too large');
            if (packets === 0) {
                if (packet[0] !== 1) fail('not a Vorbis stream');
                send(packet);
                x.p(decoder, 1);
                if (failed()) fail('bad identification header');
            } else if (packets === 2) {
                if (packet[0] !== 5) fail('missing setup header');
                send(packet);
                x.p(decoder, 0);
                if (failed()) fail('bad setup header');
                x.q(decoder);
                if (failed()) fail('could not start decoding');
                channels = heap().getInt32(channelsAt, true);
                rate = heap().getInt32(rateAt, true);
                if (channels < 1 || channels > 32 || rate < 3000 || rate > 384000) fail('unsupported format');
                allocate(lastGranule > 0 && lastGranule * channels <= maxSamples ? lastGranule : 4096);
            } else if (packets > 2) {
                send(packet);
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

        let partial = [];
        for (const page of pages) {
            const segments = bytes[page.at + 26];
            let body = page.at + 27 + segments;
            for (let i = 0; i < segments; i++) {
                const length = bytes[page.at + 27 + i];
                partial.push(bytes.subarray(body, body + length));
                body += length;
                if (length === 255) continue;
                let size = 0;
                for (const part of partial) size += part.length;
                if (size > maxPacket) fail('packet too large');
                const packet = new Uint8Array(size);
                let offset = 0;
                for (const part of partial) { packet.set(part, offset); offset += part.length; }
                partial = [];
                take(packet);
            }
        }
        x.s(decoder);
        if (!frames) fail('no audio');
        if (lastGranule > 0 && lastGranule < frames) frames = lastGranule;
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

    function decode(load, wav) {
        let job;
        const promise = new Promise((resolve, reject) => { job = { load, wav, resolve, reject, waiting: 1, abort: null, done: false }; });
        queue.push(job);
        pump();
        return { job, promise };
    }

    function pump() {
        while (queue.length && running < config.limits.workers) {
            const job = queue.shift();
            running++;
            run(job).finally(() => { running--; job.done = true; pump(); });
        }
    }

    async function run(job) {
        try {
            const bytes = await job.load();
            const module = await wasm;
            if (job.waiting <= 0) throw aborted('nobody is waiting for this audio');
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
            const result = await new Promise((resolve, reject) => {
                job.abort = () => { worker.terminate(); worker = null; reject(aborted('nobody is waiting for this audio')); };
                worker.onerror = (event) => { worker.terminate(); worker = null; reject(new Error(event.message || 'decoder failed')); };
                worker.onmessage = ({ data }) => (data.error ? reject(new Error(data.error)) : resolve(data));
                worker.postMessage({ bytes, maxSamples: config.limits.maxSamples, wav: job.wav }, [bytes]);
            }).finally(() => { if (worker) idle.push(worker); });
            job.resolve(result);
        } catch (e) {
            job.reject(e);
        }
    }

    function cancel(job) {
        if (job.done || --job.waiting > 0) return;
        const index = queue.indexOf(job);
        if (index >= 0) {
            queue.splice(index, 1);
            job.done = true;
            job.reject(aborted('nobody is waiting for this audio'));
        } else if (job.abort) {
            job.abort();
        }
    }

    function hurry(job) {
        const index = queue.indexOf(job);
        if (index > 0) queue.unshift(queue.splice(index, 1)[0]);
    }

    async function fetchOgg(url) {
        const response = await fetch(url);
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
            const bytes = buffer instanceof ArrayBuffer ? new Uint8Array(buffer) : null;
            if (!bytes || !isOggBytes(bytes) || bytes.length > config.limits.maxInputBytes) return nativeDecode.apply(this, arguments);
            const copy = bytes.slice().buffer;
            const promise = decode(() => Promise.resolve(copy), false).promise.then((result) => {
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
    // cache holds more than config.limits.maxCacheBytes.
    const tracks = new Map();
    let cachedBytes = 0, uses = 0;

    function track(url) {
        let entry = tracks.get(url);
        if (!entry) {
            entry = { users: 0, size: 0, blobURL: null, used: 0 };
            const { job, promise } = decode(() => fetchOgg(url), true);
            entry.job = job;
            entry.promise = promise.then((result) => {
                entry.blobURL = URL.createObjectURL(new Blob([result.wav], { type: 'audio/wav' }));
                entry.size = result.wav.byteLength;
                cachedBytes += entry.size;
                return entry.blobURL;
            }, (error) => {
                tracks.delete(url);
                throw error;
            });
            tracks.set(url, entry);
        } else if (!entry.blobURL) {
            entry.job.waiting++;
        }
        entry.users++;
        entry.used = ++uses;
        trim();
        return entry;
    }

    function trim() {
        while (cachedBytes > config.limits.maxCacheBytes) {
            let oldest = null;
            for (const [url, entry] of tracks) {
                if (entry.blobURL && entry.users === 0 && (!oldest || entry.used < oldest[1].used)) oldest = [url, entry];
            }
            if (!oldest) return;
            tracks.delete(oldest[0]);
            URL.revokeObjectURL(oldest[1].blobURL);
            cachedBytes -= oldest[1].size;
        }
    }

    const proto = HTMLMediaElement.prototype;
    const src = Object.getOwnPropertyDescriptor(proto, 'src');
    const nativePlay = proto.play, nativePause = proto.pause, nativeLoad = proto.load, nativeCanPlayType = proto.canPlayType;
    // Per element: the .ogg URL it was given, its cache entry, and play() calls made before the decode finished.
    const elements = new WeakMap();
    const released = new FinalizationRegistry((state) => release(state));

    function release(state) {
        const entry = state.entry;
        if (!entry) return;
        state.entry = null;
        state.url = null;
        if (state.pending) {
            state.pending = false;
            state.wantsPlay = false;
            for (const waiter of state.waiters.splice(0)) waiter.reject(aborted('The play() request was interrupted by a new load request.'));
            cancel(entry.job);
        }
        entry.users--;
        trim();
    }

    function assign(element, url) {
        let state = elements.get(element);
        if (!state) {
            state = { url: null, entry: null, pending: false, wantsPlay: false, waiters: [] };
            elements.set(element, state);
            released.register(element, state);
        }
        // Taking the new entry first keeps a decode of the same URL alive when the element asks for it again.
        const entry = track(url);
        release(state);
        state.url = url;
        state.entry = entry;
        // Stop what the element played before, as a new src would, without giving it an empty one, which is an error.
        element.removeAttribute('src');
        nativeLoad.call(element);
        if (entry.blobURL) return src.set.call(element, entry.blobURL);
        state.pending = true;
        entry.promise.catch((error) => {
            // The original bytes are no worse than nothing, and a format this cannot read may still play.
            console.warn('Ogg decode failed for ' + url + ': ' + error.message);
            return url;
        }).then((source) => {
            if (state.entry !== entry) return;
            state.pending = false;
            src.set.call(element, source);
            const waiters = state.waiters.splice(0);
            if (!state.wantsPlay) return;
            state.wantsPlay = false;
            nativePlay.call(element).then((v) => waiters.forEach((w) => w.resolve(v)), (e) => waiters.forEach((w) => w.reject(e)));
        });
    }

    Object.defineProperty(proto, 'src', {
        configurable: true,
        enumerable: true,
        get() {
            const state = elements.get(this);
            return state && state.url ? state.url : src.get.call(this);
        },
        set(value) {
            const url = absolute(value);
            if (url && isOggURL(url)) return assign(this, url);
            const state = elements.get(this);
            if (state) release(state);
            src.set.call(this, value);
        },
    });
    proto.play = function () {
        const state = elements.get(this);
        if (!state || !state.pending) return nativePlay.apply(this, arguments);
        state.wantsPlay = true;
        hurry(state.entry.job);
        return new Promise((resolve, reject) => state.waiters.push({ resolve, reject }));
    };
    proto.pause = function () {
        const state = elements.get(this);
        if (state && state.pending) {
            state.wantsPlay = false;
            for (const waiter of state.waiters.splice(0)) waiter.reject(aborted('The play() request was interrupted by a call to pause().'));
        }
        return nativePause.apply(this, arguments);
    };
    proto.load = function () {
        const state = elements.get(this);
        if (state && state.pending) return;
        return nativeLoad.apply(this, arguments);
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
})();
