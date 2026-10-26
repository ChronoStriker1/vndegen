// NW.js compatibility layer for HTML5 games running in VNDegen's WKWebView.
// Injected at document start, before the game's own scripts. Engines decide they are on NW.js when `require` is a
// function and `process` is an object, and then read and write their folder with `fs` the way the desktop build does.
// Synchronous file calls go through window.prompt, which the app answers in its WKUIDelegate. That is the only channel
// WebKit offers that blocks the page until the host replies.
(function () {
    'use strict';
    const config = __VNDEGEN_CONFIG__;
    const BRIDGE = '\u0001vndegen:';
    const nativePrompt = window.prompt.bind(window);

    function post(message) {
        try { window.webkit.messageHandlers.vndegen.postMessage(message); } catch (e) { /* no host */ }
    }

    function nodeError(code, message, syscall, path) {
        const err = new Error(code + ': ' + message + ', ' + syscall + " '" + path + "'");
        err.code = code;
        err.errno = -1;
        err.syscall = syscall;
        err.path = path;
        return err;
    }

    function call(op, args) {
        const raw = nativePrompt(BRIDGE + JSON.stringify(Object.assign({ op: op }, args)));
        if (raw == null) throw nodeError('EIO', 'bridge unavailable', op, args.path || args.from || '');
        const reply = JSON.parse(raw);
        if (reply.error) throw nodeError(reply.error.code, reply.error.message, op, args.path || args.from || '');
        return reply.value;
    }

    // path: Node's posix implementation, the subset the engine and plugins use.
    function normalize(p) {
        if (p === '') return '.';
        const absolute = p.charAt(0) === '/';
        const trailing = p.length > 1 && p.charAt(p.length - 1) === '/';
        const out = [];
        for (const part of p.split('/')) {
            if (part === '' || part === '.') continue;
            if (part === '..') {
                if (out.length && out[out.length - 1] !== '..') out.pop();
                else if (!absolute) out.push('..');
            } else {
                out.push(part);
            }
        }
        let result = out.join('/');
        if (!result && !absolute) result = '.';
        if (trailing && result && result !== '.') result += '/';
        return (absolute ? '/' : '') + result;
    }

    function stripTrailing(p) {
        let end = p.length;
        while (end > 1 && p.charAt(end - 1) === '/') end--;
        return p.slice(0, end);
    }

    const path = {
        sep: '/',
        delimiter: ':',
        normalize: normalize,
        isAbsolute: (p) => String(p).charAt(0) === '/',
        join: (...parts) => {
            const joined = parts.map(String).filter((x) => x !== '').join('/');
            return joined === '' ? '.' : normalize(joined);
        },
        resolve: (...parts) => {
            let resolved = '';
            for (let i = parts.length - 1; i >= 0 && resolved.charAt(0) !== '/'; i--) {
                const part = String(parts[i]);
                if (part) resolved = resolved ? part + '/' + resolved : part;
            }
            if (resolved.charAt(0) !== '/') resolved = config.cwd + (resolved ? '/' + resolved : '');
            return stripTrailing(normalize(resolved));
        },
        dirname: (p) => {
            p = String(p);
            if (p === '') return '.';
            const trimmed = stripTrailing(p);
            const i = trimmed.lastIndexOf('/');
            if (i < 0) return '.';
            if (i === 0) return '/';
            return stripTrailing(trimmed.slice(0, i));
        },
        basename: (p, ext) => {
            const trimmed = stripTrailing(String(p));
            let base = trimmed.slice(trimmed.lastIndexOf('/') + 1);
            if (ext && base !== ext && base.endsWith(ext)) base = base.slice(0, -ext.length);
            return base;
        },
        extname: (p) => {
            const base = path.basename(p);
            const i = base.lastIndexOf('.');
            return i <= 0 ? '' : base.slice(i);
        },
        relative: (from, to) => {
            const a = path.resolve(from).split('/').filter(Boolean);
            const b = path.resolve(to).split('/').filter(Boolean);
            let i = 0;
            while (i < a.length && i < b.length && a[i] === b[i]) i++;
            return a.slice(i).map(() => '..').concat(b.slice(i)).join('/');
        },
        parse: (p) => {
            const base = path.basename(p);
            const ext = path.extname(p);
            return { root: path.isAbsolute(p) ? '/' : '', dir: path.dirname(p), base: base, ext: ext, name: base.slice(0, base.length - ext.length) };
        },
    };
    path.posix = path;

    const absolute = (p) => path.resolve(p instanceof URL ? p.pathname : String(p));

    // Buffer: just enough for readFileSync without an encoding and for plugins that call toString on it.
    function binaryString(bytes) {
        let s = '';
        for (let i = 0; i < bytes.length; i += 8192) s += String.fromCharCode.apply(null, bytes.subarray(i, i + 8192));
        return s;
    }

    class Buffer extends Uint8Array {
        static from(value, encoding) {
            if (typeof value === 'string') {
                if (encoding === 'base64') return new Buffer(Uint8Array.from(atob(value), (c) => c.charCodeAt(0)));
                if (encoding === 'latin1' || encoding === 'binary') return new Buffer(Uint8Array.from(value, (c) => c.charCodeAt(0) & 255));
                return new Buffer(new TextEncoder().encode(value));
            }
            return new Buffer(value);
        }
        static alloc(size) { return new Buffer(size); }
        static isBuffer(value) { return value instanceof Buffer; }
        static concat(list) {
            const total = list.reduce((n, b) => n + b.length, 0);
            const out = new Buffer(total);
            let offset = 0;
            for (const b of list) { out.set(b, offset); offset += b.length; }
            return out;
        }
        toString(encoding) {
            if (encoding === 'base64') return btoa(binaryString(this));
            if (encoding === 'latin1' || encoding === 'binary') return binaryString(this);
            if (encoding === 'hex') return Array.from(this, (b) => b.toString(16).padStart(2, '0')).join('');
            return new TextDecoder().decode(this);
        }
    }

    // fs: the sync calls are real round trips to the app. Async forms run the sync one and call back on a microtask.
    function encodingOf(options) {
        const enc = typeof options === 'string' ? options : options && options.encoding;
        if (!enc) return null;
        const lower = String(enc).toLowerCase().replace('-', '');
        if (lower === 'utf8') return 'utf8';
        if (lower === 'binary') return 'latin1';
        return lower;
    }

    function stats(info) {
        return {
            size: info.size,
            mtime: new Date(info.mtimeMs),
            mtimeMs: info.mtimeMs,
            isFile: () => info.isFile,
            isDirectory: () => info.isDirectory,
            isSymbolicLink: () => false,
        };
    }

    const fs = {
        existsSync(p) {
            try { return call('exists', { path: absolute(p) }); } catch (e) { return false; }
        },
        statSync(p) { return stats(call('stat', { path: absolute(p) })); },
        lstatSync(p) { return fs.statSync(p); },
        readFileSync(p, options) {
            const enc = encodingOf(options);
            if (enc === 'utf8') return call('readFile', { path: absolute(p), text: true });
            const buffer = Buffer.from(call('readFile', { path: absolute(p), text: false }), 'base64');
            return enc ? buffer.toString(enc) : buffer;
        },
        writeFileSync(p, data, options) {
            const enc = encodingOf(options);
            if (typeof data === 'string' && (!enc || enc === 'utf8')) {
                call('writeFile', { path: absolute(p), data: data, text: true });
            } else {
                const bytes = typeof data === 'string' ? Buffer.from(data, enc) : Buffer.from(data);
                call('writeFile', { path: absolute(p), data: bytes.toString('base64'), text: false });
            }
        },
        appendFileSync(p, data, options) {
            let existing = '';
            try { existing = fs.readFileSync(p, 'utf8'); } catch (e) { /* new file */ }
            fs.writeFileSync(p, existing + String(data), options);
        },
        mkdirSync(p, options) { call('mkdir', { path: absolute(p), recursive: !!(options && options.recursive) }); },
        readdirSync(p) { return call('readdir', { path: absolute(p) }); },
        renameSync(from, to) { call('rename', { from: absolute(from), to: absolute(to) }); },
        unlinkSync(p) { call('unlink', { path: absolute(p) }); },
        rmSync(p) { fs.unlinkSync(p); },
        copyFileSync(from, to) { call('copyFile', { from: absolute(from), to: absolute(to) }); },
        accessSync(p) { if (!fs.existsSync(p)) throw nodeError('ENOENT', 'no such file or directory', 'access', String(p)); },
        constants: { F_OK: 0, R_OK: 4, W_OK: 2, X_OK: 1 },
        promises: {},
    };

    for (const name of ['stat', 'lstat', 'readFile', 'writeFile', 'appendFile', 'mkdir', 'readdir', 'rename', 'unlink', 'rm', 'copyFile', 'access']) {
        const sync = fs[name + 'Sync'];
        fs[name] = (...args) => {
            const callback = typeof args[args.length - 1] === 'function' ? args.pop() : null;
            let result, error = null;
            try { result = sync(...args); } catch (e) { error = e; }
            if (callback) queueMicrotask(() => callback(error, result));
            else return error ? Promise.reject(error) : Promise.resolve(result);
        };
        fs.promises[name] = (...args) => fs[name](...args);
    }
    fs.exists = (p, callback) => queueMicrotask(() => callback(fs.existsSync(p)));

    const os = {
        EOL: '\n',
        platform: () => config.platform,
        type: () => 'Darwin',
        arch: () => 'arm64',
        release: () => '',
        hostname: () => 'vndegen',
        homedir: () => config.cwd,
        tmpdir: () => config.cwd,
        cpus: () => [],
        totalmem: () => 0,
        freemem: () => 0,
    };

    let quitting = false;
    function quit() {
        if (quitting) return;
        quitting = true;
        try { call('quit', {}); } catch (e) { /* session already gone */ }
    }

    const noop = () => {};
    const listeners = {};
    const nwWindow = {
        window: window,
        x: 0, y: 0, width: window.innerWidth, height: window.innerHeight, zoomLevel: 0, title: document.title, menu: null,
        isFullscreen: false, isKioskMode: false, isAlwaysOnTop: false,
        on(event, fn) { (listeners[event] || (listeners[event] = [])).push(fn); return nwWindow; },
        once(event, fn) { return nwWindow.on(event, function wrapped() { nwWindow.removeListener(event, wrapped); fn.apply(null, arguments); }); },
        removeListener(event, fn) { listeners[event] = (listeners[event] || []).filter((f) => f !== fn); return nwWindow; },
        removeAllListeners(event) { if (event) delete listeners[event]; else for (const k in listeners) delete listeners[k]; return nwWindow; },
        emit(event, ...args) { for (const fn of (listeners[event] || []).slice()) fn(...args); },
        // NW.js: a close listener owns the decision, and close(true) forces it. main.js registers one that quits.
        close(force) { if (force || !(listeners.close && listeners.close.length)) quit(); else nwWindow.emit('close'); },
        reload() { location.reload(); },
        reloadDev: noop, reloadIgnoringCache() { location.reload(); },
        showDevTools: noop, closeDevTools: noop, isDevToolsOpen: () => false,
        focus: noop, blur: noop, show: noop, hide: noop, maximize: noop, minimize: noop, unmaximize: noop, restore: noop,
        enterFullscreen: noop, leaveFullscreen: noop, toggleFullscreen: noop, enterKioskMode: noop, leaveKioskMode: noop, toggleKioskMode: noop,
        setPosition: noop, moveTo: noop, moveBy: noop, resizeTo: noop, resizeBy: noop, setMinimumSize: noop, setMaximumSize: noop,
        setResizable: noop, setAlwaysOnTop: noop, setVisibleOnAllWorkspaces: noop, setShowInTaskbar: noop, setTransparent: noop,
        setBadgeLabel: noop, setProgressBar: noop, setShadow: noop, requestAttention: noop, setMenu: noop,
        capturePage(callback) { if (typeof callback === 'function') callback(''); },
        eval: noop, evalNWBin: noop,
        cookies: { get: (d, cb) => cb && cb(null), getAll: (d, cb) => cb && cb([]), set: (d, cb) => cb && cb(null), remove: (d, cb) => cb && cb(null), onChanged: { addListener: noop, removeListener: noop } },
    };

    class Menu { constructor() { this.items = []; } append(item) { this.items.push(item); } insert(item, i) { this.items.splice(i, 0, item); } remove(item) { this.items = this.items.filter((x) => x !== item); } removeAt(i) { this.items.splice(i, 1); } popup() {} createMacBuiltin() {} }
    class MenuItem { constructor(options) { Object.assign(this, options); } }
    class Shortcut { constructor(options) { Object.assign(this, options); } }
    class Tray { constructor(options) { Object.assign(this, options); } remove() {} }

    const nw = {
        App: {
            argv: [], fullArgv: [], filteredArgv: [], manifest: { name: config.appName, main: 'index.html' }, dataPath: config.cwd,
            quit: quit, closeAllWindows: quit, on: noop, once: noop, removeListener: noop, removeAllListeners: noop,
            clearCache: noop, clearAppCache: noop, getProxyForURL: () => 'DIRECT', setProxyConfig: noop,
            registerGlobalHotKey: noop, unregisterGlobalHotKey: noop, crashBrowser: noop, crashRenderer: noop, enableComponent: noop,
        },
        Window: {
            get: () => nwWindow,
            open: (url, options, callback) => { if (url) nw.Shell.openExternal(url); if (typeof callback === 'function') callback(nwWindow); },
        },
        Shell: {
            openExternal: (url) => { try { call('openExternal', { url: String(url) }); } catch (e) { /* refused */ } },
            openItem: noop,
            showItemInFolder: noop,
        },
        Screen: { Init: noop, screens: [], on: noop, removeListener: noop, DesktopCaptureMonitor: null },
        Clipboard: { get: () => ({ get: () => '', set: noop, clear: noop, readAvailableTypes: () => [] }) },
        Menu: Menu, MenuItem: MenuItem, Shortcut: Shortcut, Tray: Tray,
        process: null,
        require: null,
    };
    nw.gui = nw;

    const process = {
        platform: config.platform,
        arch: 'arm64',
        version: 'v20.0.0',
        versions: { node: '20.0.0', nw: '0.90.0', 'node-webkit': '0.90.0', chromium: '122.0.0.0', v8: '12.0.0' },
        env: {},
        argv: [config.mainModule],
        execArgv: [],
        execPath: config.mainModule,
        pid: 1,
        ppid: 0,
        title: 'nw',
        mainModule: { filename: config.mainModule, id: '.', exports: {}, loaded: true },
        cwd: () => config.cwd,
        chdir: noop,
        exit: quit,
        abort: quit,
        nextTick: (fn, ...args) => queueMicrotask(() => fn(...args)),
        on: () => process, once: () => process, off: () => process, removeListener: () => process, removeAllListeners: () => process, emit: () => false,
        memoryUsage: () => ({ rss: 0, heapTotal: 0, heapUsed: 0, external: 0, arrayBuffers: 0 }),
        uptime: () => performance.now() / 1000,
        hrtime: Object.assign((prev) => {
            const ms = performance.now();
            const now = [Math.floor(ms / 1000), Math.floor((ms % 1000) * 1e6)];
            return prev ? [now[0] - prev[0], now[1] - prev[1]] : now;
        }, { bigint: () => BigInt(Math.floor(performance.now() * 1e6)) }),
        stdout: { write: (s) => console.log(String(s)), isTTY: false },
        stderr: { write: (s) => console.error(String(s)), isTTY: false },
    };
    nw.process = process;

    const modules = { fs: fs, path: path, os: os, 'nw.gui': nw, process: process, buffer: { Buffer: Buffer } };
    function require(name) {
        const key = String(name).replace(/^node:/, '');
        if (Object.prototype.hasOwnProperty.call(modules, key)) return modules[key];
        const err = new Error("Cannot find module '" + name + "'");
        err.code = 'MODULE_NOT_FOUND';
        throw err;
    }
    require.resolve = (name) => String(name);
    require.cache = {};
    require.main = process.mainModule;
    nw.require = require;

    window.require = require;
    window.process = process;
    window.nw = nw;
    window.Buffer = Buffer;
    window.global = window;
    window.close = () => nwWindow.close();
    window.chrome = window.chrome || {};
    window.chrome.runtime = window.chrome.runtime || {};
    window.chrome.runtime.reload = () => location.reload();

    // The app delivers gamepad input through the session, so the page must not read the controllers a second time.
    try {
        Object.defineProperty(Navigator.prototype, 'getGamepads', { value: () => [], configurable: true, writable: true });
    } catch (e) { /* locked down */ }

    // WebKit's WebM player can stop on the last frame of a looping clip instead of wrapping (the game's 5-frame VP9
    // animations do, over vndegen:// and from a blob alike), and then it fires no more timeupdate events. Each
    // timeupdate re-arms a timer, so the timer runs only after updates stop, and it sends a looping element that is
    // stuck at its end back to the start.
    const play = HTMLMediaElement.prototype.play;
    const stalledLoops = new WeakMap();
    function rearmLoop(media) {
        clearTimeout(stalledLoops.get(media));
        if (!media.loop || !media.duration) return;
        const remaining = Math.max(0, media.duration - media.currentTime) / (media.playbackRate || 1);
        stalledLoops.set(media, setTimeout(() => {
            if (media.loop && !media.paused && media.currentTime >= media.duration - 0.05) media.currentTime = 0;
        }, remaining * 1000 + 250));
    }
    HTMLMediaElement.prototype.play = function () {
        if (!stalledLoops.has(this)) {
            stalledLoops.set(this, 0);
            this.addEventListener('timeupdate', () => rearmLoop(this));
        }
        return play.apply(this, arguments);
    };

    // An AudioContext starts suspended and the engine resumes it only on a touch or key event, which a gamepad player
    // never produces on the web view. The app allows playback without a gesture, so every context is resumed unless
    // the session is paused. WebKit on iOS also suspends a new context once, about 100 ms after it starts running.
    const audioContexts = [];
    let audioPaused = false;
    const resumeAudio = (context) => {
        if (!audioPaused && context.state === 'suspended' && document.visibilityState === 'visible') context.resume().catch(noop);
    };
    const NativeAudioContext = window.AudioContext || window.webkitAudioContext;
    if (NativeAudioContext) {
        class AudioContext extends NativeAudioContext {
            constructor(...args) {
                super(...args);
                audioContexts.push(this);
                this.addEventListener('statechange', () => resumeAudio(this));
                resumeAudio(this);
            }
        }
        window.AudioContext = window.webkitAudioContext = AudioContext;
        document.addEventListener('visibilitychange', () => audioContexts.forEach(resumeAudio));
    }

    // MZ asks for fullscreen on F4 without catching the rejection, which would end the game on the error screen.
    const resolved = () => Promise.resolve();
    for (const name of ['requestFullscreen', 'webkitRequestFullscreen']) {
        try { Object.defineProperty(Element.prototype, name, { value: resolved, configurable: true, writable: true }); } catch (e) { /* ignore */ }
    }
    for (const name of ['exitFullscreen', 'webkitExitFullscreen']) {
        try { Object.defineProperty(Document.prototype, name, { value: resolved, configurable: true, writable: true }); } catch (e) { /* ignore */ }
    }

    // NW.js shows no context menu, and a right click is the engine's cancel button.
    document.addEventListener('contextmenu', (event) => event.preventDefault());

    // The game's own viewport tag sets only user-scalable, so the layout viewport would be 980px wide on an iPhone.
    function ensureViewport() {
        let meta = document.querySelector('meta[name="viewport"]');
        if (!meta) {
            meta = document.createElement('meta');
            meta.name = 'viewport';
            (document.head || document.documentElement).appendChild(meta);
        }
        meta.content = 'width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no, viewport-fit=cover';
    }
    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', ensureViewport);
    else ensureViewport();

    // Console and uncaught errors go to the app log, where scripts/sim.sh log can show them.
    function describe(value) {
        if (typeof value === 'string') return value;
        if (value instanceof Error) return value.stack || value.message;
        try { return JSON.stringify(value); } catch (e) { return String(value); }
    }
    for (const level of ['log', 'info', 'debug', 'warn', 'error']) {
        const original = console[level].bind(console);
        console[level] = (...args) => {
            original(...args);
            post({ level: level, message: args.map(describe).join(' ').slice(0, 4000) });
        };
    }
    window.addEventListener('error', (event) => {
        post({ level: 'error', message: event.message + ' (' + event.filename + ':' + event.lineno + ')' });
    });
    window.addEventListener('unhandledrejection', (event) => {
        post({ level: 'error', message: 'unhandled rejection: ' + describe(event.reason) });
    });

    // Input and lifecycle entry points the session calls through evaluateJavaScript.
    let lastX = 0, lastY = 0;
    const target = () => document.body || document.documentElement;

    window.__vndegen = {
        key(type, key, code, keyCode) {
            const event = new KeyboardEvent(type, { key: key, code: code, keyCode: keyCode, which: keyCode, bubbles: true, cancelable: true });
            // The init dictionary's legacy fields are not honored everywhere, and the engine reads keyCode.
            for (const name of ['keyCode', 'which']) Object.defineProperty(event, name, { get: () => keyCode });
            target().dispatchEvent(event);
        },
        pointer(phase, nx, ny, button) {
            const x = nx * window.innerWidth, y = ny * window.innerHeight;
            lastX = x; lastY = y;
            const down = phase === 'began' || phase === 'moved';
            const init = { clientX: x, clientY: y, screenX: x, screenY: y, button: button, buttons: down ? (button === 2 ? 2 : 1) : 0, bubbles: true, cancelable: true, view: window };
            const element = document.elementFromPoint(x, y) || target();
            if (phase === 'began') element.dispatchEvent(new MouseEvent('mousemove', init));
            element.dispatchEvent(new MouseEvent({ began: 'mousedown', moved: 'mousemove', ended: 'mouseup', cancelled: 'mouseup' }[phase], init));
            if (phase === 'ended' && button === 0) element.dispatchEvent(new MouseEvent('click', init));
        },
        wheel(dy) {
            target().dispatchEvent(new WheelEvent('wheel', { deltaY: dy, deltaMode: 0, clientX: lastX, clientY: lastY, bubbles: true, cancelable: true }));
        },
        setPaused(paused) {
            audioPaused = paused;
            if (paused) {
                window.__vndegen.onPause(true);
                for (const context of audioContexts) if (context.state === 'running') context.suspend();
            } else {
                audioContexts.forEach(resumeAudio);
                window.__vndegen.onPause(false);
            }
        },
        // Engine scripts replace this to stop and restart their own loop.
        onPause: noop,
        quit: quit,
    };
})();
