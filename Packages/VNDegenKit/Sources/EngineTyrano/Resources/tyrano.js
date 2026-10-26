// TyranoScript controller support. Injected at document start, after the NW.js shim.
// Buttons call the engine's own actions on TYRANO.kag.key_mouse rather than pressing keys, because each game's
// KeyConfig.js binds keys differently (one maps Enter to hiding the message window). TyranoScript 5 has keyboard focus
// for its buttons and choices (focus_up, ok, cancel), which this script uses. Older versions have none, and games build
// screens of their own from plain HTML that Tyrano 5's focus skips, so this script also keeps a focus of its own.
(function () {
    'use strict';

    const kag = () => (window.TYRANO && window.TYRANO.kag) || null;
    const keys = () => (kag() && kag().key_mouse) || null;
    const hasNativeFocus = (k) => !!(k && k.util && typeof k.focus_up === 'function');
    // Tyrano 5's focus applies when the screen has something it can focus, or it already holds the focus.
    const nativeApplies = (k) => hasNativeFocus(k) && (k.util.findFocused().length > 0 || k.util.findFocusable().length > 0);

    const isMenuOpen = () => { const menu = document.querySelector('.layer_menu'); return !!menu && getComputedStyle(menu).display !== 'none'; };
    const openRemodal = () => document.querySelector('.remodal-wrapper.remodal-is-opened');
    const isShown = (el) => !!el && el.getClientRects().length > 0 && getComputedStyle(el).display !== 'none';

    // The element this script's own focus points at.
    let focused = null;
    let firing = false;

    function fire(el, type, bubbles = true) {
        const r = el.getBoundingClientRect();
        const x = r.left + r.width / 2, y = r.top + r.height / 2;
        el.dispatchEvent(new MouseEvent(type, {
            bubbles: bubbles, cancelable: true, view: window, clientX: x, clientY: y, screenX: x, screenY: y,
            button: 0, buttons: type === 'mousedown' ? 1 : 0,
        }));
    }

    function activate(el) {
        firing = true;
        try {
            for (const type of ['mousedown', 'mouseup', 'click']) fire(el, type);
            // A text field takes keyboard focus, so a hardware or on-screen keyboard can type into it.
            if (el.matches('input, textarea, select')) el.focus();
        } finally {
            firing = false;
        }
    }

    // Tyrano binds its buttons, choices, and menu items with jQuery, so a jQuery click or mouse handler marks them.
    function isClickable(el) {
        if (el.matches('html, body, #tyrano_base, .tyrano_base, .layer_event_click')) return false;
        if (el.matches('button, a[href], [onclick], input:not([type=hidden]), textarea, select')) return true;
        const events = window.jQuery && window.jQuery._data && window.jQuery._data(el, 'events');
        return !!events && !!(events.click || events.mousedown || events.mouseup);
    }

    function candidates() {
        const scope = openRemodal() || (isMenuOpen() && document.querySelector('.layer_menu')) || document.body;
        if (!scope) return [];
        const area = innerWidth * innerHeight;
        const found = Array.from(scope.querySelectorAll('*')).filter((el) => {
            if (!isClickable(el)) return false;
            const r = el.getBoundingClientRect();
            if (r.width < 4 || r.height < 4 || r.width * r.height > area * 0.6) return false;
            if (r.right <= 0 || r.bottom <= 0 || r.left >= innerWidth || r.top >= innerHeight) return false;
            const style = getComputedStyle(el);
            if (style.visibility === 'hidden' || style.pointerEvents === 'none' || Number(style.opacity) < 0.05) return false;
            // Skip buttons something else covers, such as the title screen's under an open menu.
            const hit = document.elementFromPoint(r.left + r.width / 2, r.top + r.height / 2);
            return !!hit && (hit === el || el.contains(hit) || hit.contains(el));
        });
        // Keep the innermost of nested candidates. A menu item is an <a> around the <img> that has the click handler,
        // and a click on the <a> never reaches it.
        return found.filter((el) => !found.some((other) => other !== el && el.contains(other))).sort((a, b) => {
            const ra = a.getBoundingClientRect(), rb = b.getBoundingClientRect();
            return Math.round(ra.top / 8) - Math.round(rb.top / 8) || ra.left - rb.left;
        });
    }

    function setFocus(el) {
        if (focused === el) return;
        if (focused) {
            focused.classList.remove('vndegen-focus');
            if (focused.isConnected) { fire(focused, 'mouseout'); fire(focused, 'mouseleave', false); }
        }
        focused = el;
        if (el) {
            el.classList.add('vndegen-focus');
            // Tyrano swaps a button's image on hover, which shows the player where the focus is.
            fire(el, 'mouseover');
            fire(el, 'mouseenter', false);
        }
    }

    function focusIsValid() { return !!focused && focused.isConnected && candidates().includes(focused); }

    function moveFocus(direction) {
        const list = candidates();
        if (!list.length) return setFocus(null);
        if (!focusIsValid()) return setFocus(direction === 'up' || direction === 'left' ? list[list.length - 1] : list[0]);
        const center = (el) => { const r = el.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; };
        const from = center(focused);
        let best = null, bestScore = Infinity;
        for (const el of list) {
            if (el === focused) continue;
            const to = center(el), dx = to.x - from.x, dy = to.y - from.y;
            const along = { up: -dy, down: dy, left: -dx, right: dx }[direction];
            const across = direction === 'up' || direction === 'down' ? Math.abs(dx) : Math.abs(dy);
            if (along <= 1) continue;
            const score = along + across * 2;
            if (score < bestScore) { best = el; bestScore = score; }
        }
        if (best) setFocus(best);
    }

    // A [button role=...] keeps its tag parameters as JSON in data-event-pm.
    function roleOf(el) {
        try { return JSON.parse(el.getAttribute('data-event-pm') || '{}').role || ''; } catch (e) { return ''; }
    }

    // Opens the menu the game offers. Many games hide Tyrano's built-in menu with [hidemenubutton] and put their own
    // buttons on screen instead (save, load, settings, or a gear), which can mean anything, so then the focus goes to
    // them, starting at Save, and the player picks.
    const menuRoles = ['save', 'load', 'config', 'sleepgame', 'backlog'];
    function openGameMenu(k) {
        const list = candidates();
        const menu = list.find((el) => roleOf(el) === 'menu');
        if (menu) return activate(menu);
        if (kag().stat.visible_menu_button !== false) return k.menu();
        const first = menuRoles.map((role) => list.find((el) => roleOf(el) === role)).find(Boolean) || list[0];
        if (first) setFocus(first);
    }

    function closeMenu(k) {
        const close = document.querySelector('.layer_menu .menu_close');
        if (close && isShown(close)) activate(close);
        else k.showmenu();
    }

    const actions = {
        confirm(k) {
            if (focusIsValid()) return activate(focused);
            // A confirmation dialog with nothing focused in it takes confirm as its OK, like Enter on a default button.
            const remodal = openRemodal();
            const nativeFocused = hasNativeFocus(k) && k.util.findFocused().length > 0;
            if (remodal && !nativeFocused) {
                setFocus(null);
                const ok = remodal.querySelector('.remodal-confirm');
                if (ok) activate(ok);
                return;
            }
            // Only a game stopped on [s] waits for a button. Otherwise it is between lines, and confirm must not wander
            // onto buttons that are always on screen, such as Auto.
            const waitsOnButtons = kag().stat.is_strong_stop;
            if (nativeApplies(k)) {
                setFocus(null);
                // ok() clicks the focused button or advances the text.
                if (!k.ok() && waitsOnButtons && !k.util.findFocused().length) k.focus_next();
                return;
            }
            const canClick = hasNativeFocus(k) ? k.util.canClick() : k.canClick();
            if (canClick) return k.next();
            const list = candidates();
            if (waitsOnButtons && list.length) setFocus(list[0]);
        },
        cancel(k) {
            if (hasNativeFocus(k) && (k.util.findFocused().length || k.util.isRemodalDisplayed() || k.util.isMenuDisplayed())) {
                setFocus(null);
                return k.cancel();
            }
            const remodal = openRemodal();
            if (remodal) {
                const cancel = remodal.querySelector('.remodal-cancel');
                const answer = cancel && isShown(cancel) ? cancel : remodal.querySelector('.remodal-confirm');
                if (answer) activate(answer);
            } else if (isMenuOpen()) {
                setFocus(null);
                closeMenu(k);
            } else if (focusIsValid()) {
                setFocus(null);
            } else {
                openGameMenu(k);
            }
        },
        menu(k) {
            setFocus(null);
            if (isMenuOpen()) return hasNativeFocus(k) ? k.menu() : closeMenu(k);
            openGameMenu(k);
        },
        pageUp(k) { k.backlog(); },
        pageDown(k) { k.auto(); },
    };
    for (const direction of ['up', 'down', 'left', 'right']) {
        actions[direction] = (k) => {
            if (!focusIsValid() && nativeApplies(k)) {
                setFocus(null);
                k['focus_' + direction]();
                // Tyrano 5 ranks buttons on hidden pages too, such as save slots on another page, and then focuses
                // nothing. This script's own focus only sees what is on screen.
                if (k.util.findFocused().length) return;
            }
            moveFocus(direction);
        };
    }

    window.__vndegenTyrano = {
        // What the controller can focus right now, for DEBUG input scripts.
        focusTargets: candidates,
        button(name, pressed) {
            const k = keys();
            if (!k) return;
            // Dash skips while held, like Ctrl on the PC.
            if (name === 'dash') {
                const stat = kag().stat;
                if (pressed && !stat.is_skip) k.skip();
                else if (!pressed && stat.is_skip) {
                    if (typeof kag().setSkip === 'function') kag().setSkip(false);
                    else stat.is_skip = false;
                }
                return;
            }
            // TyranoScript 5 presses its virtual mouse on ok when the mouse cursor shows, and releases it, which clicks,
            // when the key comes back up. Its own key handler does the same.
            if (!pressed && name === 'confirm' && hasNativeFocus(k) && k.vmouse && k.vmouse.is_visible) return k.vmouse.leftup();
            if (pressed && actions[name]) actions[name](k);
        },
    };

    // A real or overlay tap means the player stopped using the controller focus.
    document.addEventListener('mousedown', () => { if (!firing) setFocus(null); }, true);

    const style = document.createElement('style');
    style.textContent = '.vndegen-focus { outline: 3px solid #fff !important; outline-offset: 2px; ' +
        'box-shadow: 0 0 0 6px rgba(64, 156, 255, 0.85) !important; }';
    document.addEventListener('DOMContentLoaded', () => document.head.appendChild(style));
})();
