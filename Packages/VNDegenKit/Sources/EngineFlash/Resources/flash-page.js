// Runs at the start of every page, before ruffle.js: Ruffle's settings for this app, and, for a game's own HTML
// launcher, what it takes for the launcher's movies to play as they did in a browser with the Flash plugin.
(() => {
    'use strict';
    window.RufflePlayer = window.RufflePlayer || {};
    window.RufflePlayer.config = {
        publicPath: location.origin + '/__vndegen/ruffle/',
        autoplay: 'on',
        unmuteOverlay: 'hidden',
        splashScreen: false,
        letterbox: 'on',
        // The PC projector has no context menu worth showing, and a right click belongs to the game.
        contextMenu: 'off',
        // On, as in the PC projector, so Ruffle lists a movie's own context menu items, which VNDegen shows as Game
        // commands. Ruffle's own menu stays off above.
        menu: true,
        showSwfDownload: false,
        warnOnUnsupportedContent: false,
        allowFullscreen: false,
        // Links to the developer's site open in the system browser through the session, never in the game's view.
        openUrlMode: 'deny',
        upgradeToHttps: false,
        // Device text in the browser's fonts, which include VNDegen's Windows font substitutes under their Windows
        // names. Ruffle's own fallback font has no Japanese.
        deviceFontRenderer: 'canvas',
        // Ruffle prefers its wgpu renderer, whose shaders WebKit on Apple devices fails to translate, which stops the
        // movie. Its WebGL renderer draws the same.
        preferredRenderer: 'webgl',
        // Ruffle would draw at low quality on a phone or tablet, which these can afford.
        quality: 'high',
        // Lets movies send fscommand, such as a projector's quit, and call the page through ExternalInterface. The
        // page is the game's own, as the scripts of the web engines' games are.
        allowScriptAccess: true,
        logLevel: 'warn',
    };

    // Flash's generic fonts, _sans, _serif, and _typewriter and their Japanese names, as Japanese Windows draws them:
    // one family each, whose Latin faces are the Windows Latin fonts and whose other faces are the Japanese ones.
    const fonts = __VNDEGEN_FLASH_FONTS__;
    const latin = 'U+0000-024F, U+1E00-1EFF, U+2000-206F, U+20A0-20CF, U+2100-214F';
    const loads = [];
    for (const family of Object.values(fonts.families)) {
        for (const face of family.faces) {
            const source = 'local(' + JSON.stringify(face.local) + '), url(' + JSON.stringify(fonts.base + encodeURIComponent(face.file)) + ')';
            const font = new FontFace(family.name, source, {
                weight: face.bold ? 'bold' : 'normal',
                unicodeRange: face.latin ? latin : 'U+0250-1DFF, U+1F00-1FFF, U+2070-209F, U+20D0-20FF, U+2150-10FFFF',
            });
            document.fonts.add(font);
            if (!face.bold) loads.push(font.load().catch((e) => console.warn('font ' + family.name + ': ' + e)));
        }
    }
    const generic = (slot) => (fonts.families[slot] ? [fonts.families[slot].name] : []);
    window.RufflePlayer.config.defaultFonts = {
        sans: generic('sans'), serif: generic('serif'), typewriter: generic('typewriter'),
        japaneseGothic: generic('sans'), japaneseGothicMono: generic('typewriter'), japaneseMincho: generic('serif'),
    };
    // Settles once the regular faces have loaded, so text Ruffle draws first isn't drawn in a font that is still loading.
    window.__vndegenFlashFonts = Promise.all(loads);

    // A movie may name a font the device doesn't have, such as one from an Office install. Flash Player on Windows then
    // drew it in the system's UI font, so every font Ruffle asks for falls back to the sans family here.
    const fallback = generic('sans').map((name) => JSON.stringify(name))[0];
    for (const context of [window.OffscreenCanvasRenderingContext2D, window.CanvasRenderingContext2D]) {
        const property = context && Object.getOwnPropertyDescriptor(context.prototype, 'font');
        if (!property || !fallback) continue;
        Object.defineProperty(context.prototype, 'font', {
            ...property,
            set(value) { property.set.call(this, String(value).includes(fallback) ? value : value + ', ' + fallback); },
        });
    }

    // Ruffle names a shared object `<host>/<local path>/<name>`, with the local path as the movie's URL has it, so the
    // same movie saves under different keys when pages encode its path differently, as `Folder%21` and `Folder!`. Each
    // key goes to storage in one form: the local path decoded and encoded again the way the app encodes movie URLs,
    // which is how SharedObjectStore names it when it reads the save back. In a raw key a literal # can only start the
    // name, since a URL path encodes it, so the first part that starts with # is where the name begins.
    const host = location.hostname;
    const keep = (character) => {
        const code = character.codePointAt(0);
        return code >= 0x21 && code <= 0x7e && !'"#%/<>?`{}'.includes(character);
    };
    const encodePart = (part) => Array.from(part).map((c) => (keep(c) ? c : encodeURIComponent(c))).join('');
    const decodePart = (part) => { try { return decodeURIComponent(part); } catch (e) { return part; } };
    const canonical = (key) => {
        if (typeof key !== 'string' || !key.startsWith(host + '/')) return key;
        const parts = key.slice(host.length + 1).split('/');
        if (parts[0] === '' && parts.length >= 2) parts.shift();
        const marked = parts.findIndex((part) => part.startsWith('#'));
        const split = marked >= 0 ? marked : parts.length - 1;
        const path = parts.slice(0, split).map((part) => encodePart(decodePart(part)));
        const name = parts.slice(split).join('/');
        return host + '/' + (path.length ? path.join('/') + '/' : '/') + name;
    };
    const storage = window.localStorage;
    if (storage) {
        const methods = { getItem: (key) => storage.getItem(canonical(key)), setItem: (key, value) => storage.setItem(canonical(key), value),
            removeItem: (key) => storage.removeItem(canonical(key)) };
        const ours = (key) => typeof key === 'string' && key.startsWith(host + '/');
        Object.defineProperty(window, 'localStorage', { configurable: true, enumerable: true, value: new Proxy(storage, {
            get: (target, key) => {
                if (key in methods) return methods[key];
                // This page's keys always read through their one form, whatever form an older value has.
                if (ours(key)) return target[canonical(key)];
                // WebKit's own Storage methods need it as `this`, not the proxy.
                const value = Reflect.get(target, key);
                return typeof value === 'function' ? value.bind(target) : value;
            },
            set: (target, key, value) => { target[canonical(key)] = value; return true; },
            deleteProperty: (target, key) => { delete target[canonical(key)]; return true; },
            has: (target, key) => canonical(key) in target,
        }) });
    }

    // Flash Player writes a movie's shared objects when it quits, and most games never ask for it sooner. Ruffle writes
    // them when the page hides, so a pagehide of our own writes them whenever the game may end: when it pauses, as when
    // the VNDegen menu opens or the app leaves the screen, when the page is hidden, and before the session quits.
    const flush = () => window.dispatchEvent(new PageTransitionEvent('pagehide', { persisted: true }));
    document.addEventListener('visibilitychange', () => { if (document.hidden) flush(); });

    // The player that last had focus, which the game was being played in.
    let lastFocused = null;
    document.addEventListener('focusin', (event) => {
        const player = event.target instanceof Element && event.target.closest('ruffle-player, ruffle-embed, ruffle-object');
        if (player) lastFocused = player;
    }, true);

    // Every Ruffle player on the page, the host page's own and those Ruffle puts in place of a launcher's <embed> and
    // <object> elements.
    const players = () => Array.from(document.querySelectorAll('ruffle-player, ruffle-embed, ruffle-object'))
        .filter((player) => typeof player.ruffle === 'function');
    if (window.__vndegen) {
        window.__vndegen.onPause = (paused) => {
            if (paused) flush();
            for (const player of players()) {
                if (paused) {
                    player.ruffle().suspend();
                    // VNDegen owns this pause. Ruffle's play button would show beneath the session menu.
                    const playButton = player.shadowRoot && player.shadowRoot.getElementById('play-button');
                    if (playButton) playButton.style.display = 'none';
                } else {
                    player.ruffle().resume();
                }
            }
            // Ruffle reads keys only while a player has focus, which the hub's menu takes away. The player that had it
            // gets it back.
            if (!paused) {
                const player = lastFocused && lastFocused.isConnected ? lastFocused : players()[0];
                if (player) player.focus();
            }
        };
        window.__vndegen.beforeQuit = flush;
    }

    // VNDegen's Game commands list the movie's own right-click items, which Flash Player showed and a right click here,
    // which belongs to the game, doesn't. A full-screen item is left out, since the session owns the window. The list
    // comes from Ruffle's player with VNDegen's patch (Engines/Flash/patches/0002), and each item's id is its index.
    const fullScreen = /full ?screen|フルスクリーン|ﾌﾙｽｸﾘｰﾝ/i;
    const menuPlayer = () => {
        const player = lastFocused && lastFocused.isConnected ? lastFocused : players()[0];
        return player && typeof player.movieContextMenuItems === 'function' ? player : null;
    };
    window.__vndegenFlashMenu = {
        items() {
            const player = menuPlayer();
            if (!player) return [];
            return player.movieContextMenuItems().filter((item) => !fullScreen.test(item.caption)).map((item) => ({
                id: String(item.index), title: item.caption, checked: item.checked, enabled: item.enabled,
                separatorBefore: item.separatorBefore,
            }));
        },
        choose(index) {
            const player = menuPlayer();
            if (player && index >= 0) player.runContextMenuItem(index).catch((error) => console.error('flash menu: ' + error));
        },
    };

    // A projector's Quit button sends fscommand("quit"), which ends the session as quitting the projector would. The
    // other commands set up the projector's window, which the session owns.
    const quitHandled = new WeakSet();
    const handleQuit = () => {
        for (const player of players()) {
            if (quitHandled.has(player)) continue;
            quitHandled.add(player);
            player.ruffle().addFSCommandHandler((command) => {
                if (command.toLowerCase() === 'quit' && window.__vndegen) window.__vndegen.quit();
            });
        }
    };
    new MutationObserver(handleQuit).observe(document, { childList: true, subtree: true });

    // The session's pointer, from the touch modes and controllers, arrives as synthetic mouse events and wheel events on
    // the element under it, which is the player itself. Ruffle reads pointer and wheel events on the canvas in the
    // player's shadow root, so they go there as a real mouse would send them. Real mice and fingers reach the canvas
    // themselves. The wheel events sent on are marked, since they reach this listener again on their way up.
    // The session sends wheel turns to the page's body at the pointer's place, so the player there takes them.
    const canvasOf = (event) => {
        const target = event.target instanceof Element && event.target !== document.body ? event.target
            : document.elementFromPoint(event.clientX, event.clientY);
        const player = target && target.closest('ruffle-player, ruffle-embed, ruffle-object');
        return player && player.shadowRoot && player.shadowRoot.querySelector('canvas');
    };
    const pointerTypes = { mousedown: 'pointerdown', mousemove: 'pointermove', mouseup: 'pointerup' };
    for (const type of Object.keys(pointerTypes)) {
        window.addEventListener(type, (event) => {
            const canvas = event.isTrusted ? null : canvasOf(event);
            if (!canvas) return;
            event.stopImmediatePropagation();
            if (type === 'mousedown') canvas.getRootNode().host.focus();
            canvas.dispatchEvent(new PointerEvent(pointerTypes[type], {
                pointerId: 1, pointerType: 'mouse', isPrimary: true,
                clientX: event.clientX, clientY: event.clientY, screenX: event.screenX, screenY: event.screenY,
                button: event.button, buttons: event.buttons, bubbles: true, cancelable: true, composed: true, view: window,
            }));
        }, true);
    }
    const forwarded = new WeakSet();
    window.addEventListener('wheel', (event) => {
        const canvas = event.isTrusted || forwarded.has(event) ? null : canvasOf(event);
        if (!canvas) return;
        event.stopImmediatePropagation();
        const wheel = new WheelEvent('wheel', {
            deltaX: event.deltaX, deltaY: event.deltaY, deltaMode: event.deltaMode, clientX: event.clientX, clientY: event.clientY,
            bubbles: true, cancelable: true, composed: true, view: window,
        });
        forwarded.add(wheel);
        canvas.dispatchEvent(wheel);
    }, true);

    if (location.pathname.startsWith('/__vndegen/')) return;

    // Ruffle swaps a launcher's <embed> and <object> elements for <ruffle-embed> and <ruffle-object>, which the page's
    // own style rules no longer select, so its movies lose their size and position. Each rule that names embed or
    // object gets a copy for Ruffle's elements.
    const element = /(^|[\s>+~,(])(embed|object)(?=$|[\s>+~,.#:[)])/gi;
    function copyRules(sheet) {
        let rules;
        try {
            rules = Array.from(sheet.cssRules);
        } catch (e) {
            return;
        }
        for (const rule of rules) {
            if (!rule.selectorText || !element.test(rule.selectorText)) continue;
            element.lastIndex = 0;
            const selector = rule.selectorText.replace(element, (match, before, name) => before + 'ruffle-' + name.toLowerCase());
            sheet.insertRule(selector + '{' + rule.style.cssText + '}', sheet.cssRules.length);
        }
    }
    document.addEventListener('DOMContentLoaded', () => Array.from(document.styleSheets).forEach(copyRules));

    // A link to a movie or a projector opens it in the app's player, as a browser with the plugin would play it.
    document.addEventListener('click', (event) => {
        const link = event.target instanceof Element && event.target.closest('a[href]');
        if (!link) return;
        const url = new URL(link.getAttribute('href'), location.href);
        if (url.origin !== location.origin || !/\.(swf|exe)$/i.test(url.pathname)) return;
        event.preventDefault();
        // The path stays as the page encoded it, and the query goes along, as movie parameters do in a browser.
        location.href = '/__vndegen/flash.html?movie=' + encodeURIComponent(url.pathname + url.search);
    }, true);
})();
