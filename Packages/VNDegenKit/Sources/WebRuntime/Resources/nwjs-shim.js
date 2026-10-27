// NW.js and Electron compatibility layer for HTML5 games running in VNDegen's WKWebView.
// Injected at document start, before the game's own scripts. Engines decide they are on NW.js when `require` is a
// function and `process` is an object, and then read and write their folder with `fs` the way the desktop build does.
// config.runtime picks what the page sees: 'nwjs', 'electron' (Node plus the `electron` module), or 'browser' (no Node).
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
        execPath: config.execPath,
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

    // Electron with nodeIntegration and the deprecated `remote` module, which is what Electron game templates call.
    const electronApp = {
        getAppPath: () => config.appPath,
        getPath: () => config.cwd,
        getName: () => config.appName,
        getVersion: () => '1.0.0',
        requestSingleInstanceLock: () => true,
        quit: quit, exit: quit, on: noop, once: noop,
    };
    const electron = {
        app: electronApp,
        remote: {
            app: electronApp,
            getCurrentWindow: () => nwWindow,
            BrowserWindow: { getFocusedWindow: () => nwWindow, getAllWindows: () => [nwWindow] },
            dialog: { showMessageBoxSync: () => 0, showOpenDialogSync: () => undefined, showSaveDialogSync: () => undefined },
            Menu: Menu, MenuItem: MenuItem,
        },
        shell: { openExternal: (url) => { nw.Shell.openExternal(url); return Promise.resolve(); } },
        ipcRenderer: { send: noop, on: noop, once: noop, removeListener: noop, invoke: () => Promise.resolve() },
    };

    // Modules that games ship in node_modules/ and load whether or not they use them, such as TyranoScript's patch
    // check. fs-extra's sync calls map onto fs. Unpacking zips is not supported.
    const fsExtra = Object.assign({}, fs, {
        pathExistsSync: fs.existsSync,
        ensureDirSync: (p) => fs.mkdirSync(p, { recursive: true }),
        mkdirsSync: (p) => fs.mkdirSync(p, { recursive: true }),
        mkdirpSync: (p) => fs.mkdirSync(p, { recursive: true }),
        copySync: (from, to) => fs.copyFileSync(from, to),
        removeSync: (p) => { if (fs.existsSync(p) && !fs.statSync(p).isDirectory()) fs.unlinkSync(p); },
        readJsonSync: (p) => JSON.parse(fs.readFileSync(p, 'utf8')),
        writeJsonSync: (p, value) => fs.writeFileSync(p, JSON.stringify(value)),
    });
    class AdmZip {
        constructor() { throw new Error('VNDegen cannot unpack zip files for the game'); }
    }

    const modules = {
        fs: fs, path: path, os: os, process: process, buffer: { Buffer: Buffer }, 'fs-extra': fsExtra, 'adm-zip': AdmZip,
    };
    if (config.runtime === 'electron') {
        modules.electron = electron;
        process.versions.electron = '22.0.0';
    } else {
        modules['nw.gui'] = nw;
    }
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

    if (config.runtime !== 'browser') {
        window.require = require;
        window.process = process;
        window.Buffer = Buffer;
        window.global = window;
        window.__dirname = path.dirname(config.mainModule);
        window.__filename = config.mainModule;
        window.chrome = window.chrome || {};
        window.chrome.runtime = window.chrome.runtime || {};
        window.chrome.runtime.reload = () => location.reload();
    }
    if (config.runtime === 'nwjs') window.nw = nw;
    window.close = () => nwWindow.close();

    // localStorage kept in files in the app's data directory for this game, written before setItem returns. WebKit's
    // own storage would share one origin among all games and write to disk at a time of its choosing. Memory changes
    // only after the disk does, so a failed write never leaves the page seeing less than is saved. A clear that fails
    // part way reloads what is left on disk.
    if (config.storage) {
        let items = new Map(Object.entries(call('storageLoad', {})));
        const storage = {
            get length() { return items.size; },
            key: (index) => { const keys = Array.from(items.keys()); return index < keys.length ? keys[index] : null; },
            getItem: (key) => { key = String(key); return items.has(key) ? items.get(key) : null; },
            setItem: (key, value) => {
                key = String(key); value = String(value);
                call('storageSet', { key: key, value: value });
                items.set(key, value);
            },
            removeItem: (key) => {
                key = String(key);
                if (!items.has(key)) return;
                call('storageRemove', { key: key });
                items.delete(key);
            },
            clear: () => {
                try {
                    call('storageClear', {});
                } catch (e) {
                    items = new Map(Object.entries(call('storageLoad', {})));
                    throw e;
                }
                items.clear();
            },
        };
        // Pages also read and write keys as properties, as in localStorage.foo = 'bar'.
        const named = (key) => typeof key === 'string' && !(key in storage);
        const localStorage = new Proxy(storage, {
            get: (target, key) => (named(key) ? (items.has(key) ? items.get(key) : undefined) : target[key]),
            set: (target, key, value) => { if (named(key)) target.setItem(key, value); return true; },
            deleteProperty: (target, key) => { if (named(key)) target.removeItem(key); return true; },
            has: (target, key) => key in target || items.has(key),
            ownKeys: () => Array.from(items.keys()),
            getOwnPropertyDescriptor: (target, key) => (items.has(key)
                ? { value: items.get(key), writable: true, enumerable: true, configurable: true } : undefined),
        });
        Object.defineProperty(window, 'localStorage', { value: localStorage, configurable: true, enumerable: true });
    }

    // The page sees one standard-mapping Xbox gamepad, which the session presses through __vndegen.gamepad from both the
    // touch gamepad and real controllers, after the game's button mapping. WebKit's own view of the controllers stays
    // hidden, so the page never reads them a second time. Like a browser, the page sees the gamepad from its first press.
    // Each call returns new objects, as Chrome does, since games keep the last state to find what changed.
    const padButtons = {
        a: 0, b: 1, x: 2, y: 3, l: 4, r: 5, l2: 6, r2: 7, view: 8, menu: 9, l3: 10, r3: 11,
        up: 12, down: 13, left: 14, right: 15,
    };
    // Axis index and direction. Sticks are digital: a pushed direction is all the way over.
    const padAxes = {
        leftStickLeft: [0, -1], leftStickRight: [0, 1], leftStickUp: [1, -1], leftStickDown: [1, 1],
        rightStickLeft: [2, -1], rightStickRight: [2, 1], rightStickUp: [3, -1], rightStickDown: [3, 1],
    };
    const padHeld = new Set();
    let padConnected = false, padTimestamp = 0;
    function padSnapshot() {
        const buttons = [];
        // Button 16 is Home, which always opens the VNDegen menu instead.
        for (let i = 0; i <= 16; i++) {
            const pressed = Object.keys(padButtons).some((name) => padButtons[name] === i && padHeld.has(name));
            buttons.push({ pressed: pressed, touched: pressed, value: pressed ? 1 : 0 });
        }
        const axes = [0, 0, 0, 0];
        for (const name of padHeld) if (padAxes[name]) axes[padAxes[name][0]] += padAxes[name][1];
        return {
            id: 'Xbox Wireless Controller (STANDARD GAMEPAD Vendor: 045e Product: 0b13)', index: 0, connected: true,
            mapping: 'standard', timestamp: padTimestamp, buttons: buttons, axes: axes, vibrationActuator: null,
        };
    }
    try {
        Object.defineProperty(Navigator.prototype, 'getGamepads', {
            value: () => (padConnected ? [padSnapshot()] : []), configurable: true, writable: true,
        });
    } catch (e) { /* locked down */ }
    // WebKit still announces the real controllers to pages that listen for them.
    for (const type of ['gamepadconnected', 'gamepaddisconnected']) {
        window.addEventListener(type, (event) => { if (event.isTrusted) event.stopImmediatePropagation(); }, true);
    }
    function pressPad(name, pressed) {
        if (!(name in padButtons) && !(name in padAxes)) return;
        if (pressed) padHeld.add(name); else padHeld.delete(name);
        padTimestamp = performance.now();
        if (padConnected) return;
        padConnected = true;
        const event = new Event('gamepadconnected');
        Object.defineProperty(event, 'gamepad', { value: padSnapshot() });
        window.dispatchEvent(event);
    }

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

    // A game page is one fixed screen. WebKit scrolls it to keep a focused text field above the on-screen keyboard and
    // leaves it scrolled when the keyboard goes, which would cut off the bottom of the game. Once no text field has focus,
    // the page goes back to the top.
    const editing = () => { const a = document.activeElement; return !!a && (a.isContentEditable || /^(INPUT|TEXTAREA|SELECT)$/.test(a.tagName)); };
    const unscroll = () => { if (!editing() && (window.scrollX || window.scrollY)) window.scrollTo(0, 0); };
    window.addEventListener('scroll', unscroll);
    document.addEventListener('focusout', () => setTimeout(unscroll, 0));
    if (window.visualViewport) window.visualViewport.addEventListener('resize', unscroll);

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
    const target = () => document.body || document.documentElement;

    // Where the pointer last was, from the real mouse, finger, or pen as well as from the session's own pointer events,
    // so a click bound to a controller button lands under the cursor. Null is the middle of the page.
    let lastX = null, lastY = null;
    const trackPointer = (event) => { lastX = event.clientX; lastY = event.clientY; };
    for (const type of ['pointermove', 'pointerdown']) window.addEventListener(type, trackPointer, { capture: true, passive: true });
    const pointerX = () => (lastX === null ? window.innerWidth / 2 : lastX);
    const pointerY = () => (lastY === null ? window.innerHeight / 2 : lastY);

    function dispatchPointer(phase, x, y, button) {
        lastX = x; lastY = y;
        const down = phase === 'began' || phase === 'moved';
        const init = { clientX: x, clientY: y, screenX: x, screenY: y, button: button, buttons: down ? (button === 2 ? 2 : 1) : 0, bubbles: true, cancelable: true, view: window };
        const element = document.elementFromPoint(x, y) || target();
        if (phase === 'began') element.dispatchEvent(new MouseEvent('mousemove', init));
        element.dispatchEvent(new MouseEvent({ began: 'mousedown', moved: 'mousemove', ended: 'mouseup', cancelled: 'mouseup' }[phase], init));
        if (phase === 'ended' && button === 0) element.dispatchEvent(new MouseEvent('click', init));
    }

    // A finger acts as the mouse, so a touch first moves the mouse to it, as a mouse moves before it clicks. PC games
    // can depend on that: some plugins ignore all mouse and touch input from a few seconds after the mouse last moved
    // until it moves again, and some menus select only what the mouse is over. The touch still reaches the game itself.
    // Only the start of a touch moves the mouse. Moving it along a drag would hover RPG Maker MZ's menus mid-touch,
    // and a release after a small slide would then choose the item the touch had only just selected.
    const moveMouseToTouch = (event) => {
        const touch = event.changedTouches[0];
        if (!touch) return;
        const init = { clientX: touch.clientX, clientY: touch.clientY, screenX: touch.screenX, screenY: touch.screenY, bubbles: true, cancelable: true, view: window };
        (document.elementFromPoint(touch.clientX, touch.clientY) || target()).dispatchEvent(new MouseEvent('mousemove', init));
    };
    window.addEventListener('touchstart', moveMouseToTouch, { capture: true, passive: true });

    // The text field typing goes into: the focused one, or else the only one showing, since a click the session sends
    // does not focus a field the way a real click does.
    const typable = (element) => !!element && !element.disabled && !element.readOnly
        && (element.tagName === 'TEXTAREA' || (element.tagName === 'INPUT' && /^(text|search|email|url|tel|password|)$/i.test(element.type || '')));
    function textField() {
        if (typable(document.activeElement)) return document.activeElement;
        const showing = Array.from(document.querySelectorAll('input, textarea')).filter((element) => typable(element) && element.getClientRects().length > 0);
        if (showing.length !== 1) return null;
        showing[0].focus();
        return showing[0];
    }
    function typeKey(key, code, keyCode, character) {
        const charCode = character === null ? 0 : character.charCodeAt(0);
        const types = character === null ? ['keydown', 'keyup'] : ['keydown', 'keypress', 'keyup'];
        for (const type of types) {
            // A keypress carries the character's code where the key events carry the key's.
            const legacy = type === 'keypress' ? charCode : keyCode;
            const event = new KeyboardEvent(type, { key: key, code: code, keyCode: legacy, charCode: type === 'keypress' ? charCode : 0, which: legacy, bubbles: true, cancelable: true });
            for (const name of ['keyCode', 'which']) Object.defineProperty(event, name, { get: () => legacy });
            (document.activeElement || target()).dispatchEvent(event);
        }
    }
    function typeText(kind, value) {
        const field = textField();
        if (kind === 'newline') {
            if (!field || field.tagName !== 'TEXTAREA') return typeKey('Enter', 'Enter', 13, '\r');
            kind = 'insert';
            value = '\n';
        }
        if (!field) {
            if (kind === 'delete') return typeKey('Backspace', 'Backspace', 8, null);
            for (const character of value) typeKey(character, '', character.toUpperCase().charCodeAt(0), character);
            return;
        }
        // Email fields have no selection, so typing there goes at the end.
        if (field.selectionStart === null) {
            field.value = kind === 'delete' ? Array.from(field.value).slice(0, -1).join('') : field.value + value;
        } else {
            let start = field.selectionStart, end = field.selectionEnd;
            if (kind === 'delete' && start === end && start > 0) start -= 1;
            field.setRangeText(kind === 'delete' ? '' : value, start, end, 'end');
        }
        field.dispatchEvent(new Event('input', { bubbles: true }));
    }

    // Presses held per key code. Different buttons can send the same key, such as Confirm and a button bound to Enter,
    // so the key goes down with the first press and up with the last release.
    const keysHeld = new Map();

    window.__vndegen = {
        key(type, key, code, keyCode) {
            const count = keysHeld.get(code) || 0;
            if (type === 'keydown') {
                keysHeld.set(code, count + 1);
                if (count > 0) return;
            } else {
                if (count === 0) return;
                if (count > 1) { keysHeld.set(code, count - 1); return; }
                keysHeld.delete(code);
            }
            const event = new KeyboardEvent(type, { key: key, code: code, keyCode: keyCode, which: keyCode, bubbles: true, cancelable: true });
            // The init dictionary's legacy fields are not honored everywhere, and the engine reads keyCode.
            for (const name of ['keyCode', 'which']) Object.defineProperty(event, name, { get: () => keyCode });
            target().dispatchEvent(event);
        },
        pointer(phase, nx, ny, button) {
            dispatchPointer(phase, nx * window.innerWidth, ny * window.innerHeight, button);
        },
        // A mouse button bound to a controller or touch button, pressed where the pointer is.
        click(phase, button) {
            dispatchPointer(phase, pointerX(), pointerY(), button);
        },
        wheel(dy) {
            target().dispatchEvent(new WheelEvent('wheel', { deltaY: dy, deltaMode: 0, clientX: pointerX(), clientY: pointerY(), bubbles: true, cancelable: true }));
        },
        // Typing from VNDegen's on-screen keyboard: 'insert' with the text, 'newline', or 'delete'.
        text(kind, value) {
            typeText(kind, value);
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
        // A virtual gamepad button by its VNDegen name, such as 'a' or 'leftStickUp', with the engine-neutral action
        // it stands for, such as 'confirm', or null.
        gamepad(name, pressed, action) {
            pressPad(name, pressed);
            window.__vndegen.onGamepad(action, pressed, name);
        },
        // Engine scripts replace this to stop and restart their own loop.
        onPause: noop,
        // Engine scripts whose games may read no gamepad replace this to turn its actions into their own input. It gets the
        // engine-neutral action, or null, then pressed, then the button's VNDegen name.
        onGamepad: noop,
        quit: quit,
    };
})();
