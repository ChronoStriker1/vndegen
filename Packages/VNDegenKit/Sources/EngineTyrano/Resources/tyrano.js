// TyranoScript controller support. Injected at document start, after the NW.js shim.
// In games that read no gamepad, gamepad buttons call the engine's own actions on TYRANO.kag.key_mouse rather than
// pressing keys, because each game's KeyConfig.js binds keys differently (one maps Enter to hiding the message window).
// TyranoScript 5 has keyboard focus for its buttons and choices (focus_up, ok, cancel), which this script uses. Older
// versions have none, and games build screens of their own from plain HTML that Tyrano 5's focus skips, so this script
// also keeps a focus of its own.
(function () {
    'use strict';

    const kag = () => (window.TYRANO && window.TYRANO.kag) || null;
    const keys = () => (kag() && kag().key_mouse) || null;
    const hasNativeFocus = (k) => !!(k && k.util && typeof k.focus_up === 'function');
    // Tyrano 5's focus applies when the screen has something it can focus, or it already holds the focus.
    const nativeApplies = (k) => hasNativeFocus(k) && (k.util.findFocused().length > 0 || k.util.findFocusable().length > 0);
    // Tyrano 5's focus moves only between its own buttons. A game can put a control of its own beside them, such as a
    // choice built from plain HTML next to a Save button, and then this script's own focus, which sees both, moves.
    const related = (a, b) => a === b || a.contains(b) || b.contains(a);
    const nativeReachesAll = (k) => {
        const native = k.util.findFocusable().toArray();
        return candidates().every((el) => native.some((n) => related(n, el)));
    };

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

    // Tyrano 5.2 has setters that also update its own mode effects. Older versions only keep the flag.
    const setSkip = (k, on) => { if (typeof k.setSkip === 'function') k.setSkip(on); else k.stat.is_skip = on; };
    const setAuto = (k, on) => { if (typeof k.setAuto === 'function') k.setAuto(on); else k.stat.is_auto = on; };

    // Tyrano ignores a tap on Skip or Auto while a line types or the game waits. A press also clears the running mode
    // before its click arrives, so a tap on a running button would start it again. Players expect the button to answer at
    // any moment and a second tap to turn it off, as in other visual novels, so those taps are handled here. A running line
    // picks up the flag and finishes by itself, so nothing advances the script. A game waiting for a click takes a tap that
    // starts a mode through the button's own handler, which also plays its sound.
    const modeRole = (el) => { const role = el instanceof Element ? roleOf(el) : ''; return role === 'skip' || role === 'auto' ? role : ''; };
    const runningModes = (stat) => ({ skip: stat.is_skip === true, auto: stat.is_auto === true });
    let modesAtPress = null;
    // Set only between a press's release and the click that release sends, which the browser delivers together. A press
    // that never ends in a click, or a click with no press, such as Tyrano's own focus sends, leaves nothing behind here.
    let modesBeforeClick = null;

    document.addEventListener('mousedown', (event) => {
        const k = kag();
        modesAtPress = k && k.stat && modeRole(event.target) ? runningModes(k.stat) : null;
    }, true);

    document.addEventListener('mouseup', () => {
        modesBeforeClick = modesAtPress;
        modesAtPress = null;
        setTimeout(() => { modesBeforeClick = null; }, 0);
    }, true);

    document.addEventListener('click', (event) => {
        const k = kag(), role = modeRole(event.target);
        const was = modesBeforeClick;
        modesBeforeClick = null;
        if (!k || !k.stat || !k.layer || !role) return;
        const stat = k.stat, running = was || runningModes(stat);
        const busy = !stat.is_strong_stop && (stat.is_adding_text || k.layer.layer_event.css('display') === 'none');
        if (running[role]) {
            if (role === 'skip') setSkip(k, false);
            else k.ftag.startTag('autostop', { next: 'false' });
        } else if (!busy) {
            return;
        } else if (role === 'skip') {
            if (stat.is_auto) k.ftag.startTag('autostop', { next: 'false' });
            k.readyAudio();
            setSkip(k, true);
        } else {
            setSkip(k, false);
            k.readyAudio();
            setAuto(k, true);
        }
        event.stopPropagation();
    }, true);

    // Runs for the life of the page, because the engine loads after this script and replaces its config as it does.
    setInterval(() => {
        const k = kag();
        if (!k || !k.stat) return;
        // Most games ship with unReadTextSkip off, and then Tyrano ends Skip at the first line the player hasn't read, so
        // the button seems dead on a first playthrough. A setting screen stores the player's choice in sf, and that choice
        // stands. This changes only the running engine's config, so nothing reaches the game's saves.
        const sf = k.variable && k.variable.sf;
        if (k.config && sf && sf._system_config_unread_text_skip === undefined) k.config.unReadTextSkip = 'true';
        // Older Tyrano shows nothing while Skip or Auto runs. The classes go on the page's root rather than on the buttons
        // because a save stores the buttons' HTML, which then still loads on PC exactly as the game wrote it.
        document.documentElement.classList.toggle('vndegen-skip-on', k.stat.is_skip === true);
        document.documentElement.classList.toggle('vndegen-auto-on', k.stat.is_auto === true);
    }, 200);

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
                if (k.ok() || !waitsOnButtons || k.util.findFocused().length) return;
                if (nativeReachesAll(k)) return k.focus_next();
                const list = candidates();
                if (list.length) setFocus(list[0]);
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
                if (nativeReachesAll(k)) {
                    setFocus(null);
                    k['focus_' + direction]();
                    // Tyrano 5 ranks buttons on hidden pages too, such as save slots on another page, and then focuses
                    // nothing. This script's own focus only sees what is on screen.
                    if (k.util.findFocused().length) return;
                } else {
                    // Take over from Tyrano's focus where it is, so the move starts from the button the player sees.
                    const from = k.util.findFocused()[0];
                    if (from) {
                        k.util.unfocus();
                        const start = candidates().find((el) => related(el, from));
                        if (start) setFocus(start);
                    }
                }
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
                else if (!pressed && stat.is_skip) setSkip(kag(), false);
                return;
            }
            // TyranoScript 5 presses its virtual mouse on ok when the mouse cursor shows, and releases it, which clicks,
            // when the key comes back up. Its own key handler does the same.
            if (!pressed && name === 'confirm' && hasNativeFocus(k) && k.vmouse && k.vmouse.is_visible) return k.vmouse.leftup();
            if (pressed && actions[name]) actions[name](k);
        },
    };

    // TyranoScript 5 reads a gamepad itself when the game sets useGamepad in Config.tjs, and then does what KeyConfig.js
    // binds to each button by its name or standard index. Games often turn it on and bind nothing. A button the game binds
    // is the game's alone. Every other button does its engine-neutral action here.
    const padNames = {
        a: ['A', 0], b: ['B', 1], x: ['X', 2], y: ['Y', 3], l: ['LB', 4], r: ['RB', 5], l2: ['LT', 6], r2: ['RT', 7],
        view: ['SELECT', 8], menu: ['START', 9], l3: ['LS', 10], r3: ['RS', 11],
        up: ['UP', 12], down: ['DOWN', 13], left: ['LEFT', 14], right: ['RIGHT', 15],
        leftStickUp: ['L_UP', null, 'L', 0], leftStickDown: ['L_DOWN', null, 'L', 0],
        leftStickLeft: ['L_LEFT', null, 'L', 0], leftStickRight: ['L_RIGHT', null, 'L', 0],
        rightStickUp: ['R_UP', null, 'R', 1], rightStickDown: ['R_DOWN', null, 'R', 1],
        rightStickLeft: ['R_LEFT', null, 'R', 1], rightStickRight: ['R_RIGHT', null, 'R', 1],
    };
    const readsGamepad = (k) => !!(k && k.key_mouse && String(k.config && k.config.useGamepad) === 'true');
    function gameBinds(name) {
        const k = kag();
        if (!readsGamepad(k) || !padNames[name]) return false;
        const pad = k.key_mouse.map_pad || {};
        const [label, index, stick, stickIndex] = padNames[name];
        if (stick) {
            const analog = pad.stick || {};
            return !!((pad.stick_digital || {})[label] || analog[stick] || analog[stickIndex]);
        }
        const buttons = pad.button || {};
        return !!(buttons[label] || buttons[index]);
    }

    // Tyrano starts reading the gamepad when it hears gamepadconnected, which the page sends once, at the first press.
    // A press while the game loads comes before Tyrano listens, so the pad is announced again until Tyrano reads it, for
    // up to a minute after the page starts, and at each press. Tyrano ignores the event once it reads the pad.
    function announcePad() {
        const k = kag(), pad = navigator.getGamepads()[0];
        const gamepad = readsGamepad(k) && k.key_mouse.gamepad;
        if (!pad || !gamepad || gamepad.gamepad_exests) return;
        const event = new Event('gamepadconnected');
        Object.defineProperty(event, 'gamepad', { value: pad });
        window.dispatchEvent(event);
    }
    let waits = 0;
    const waitForEngine = setInterval(() => {
        announcePad();
        const k = kag(), gamepad = readsGamepad(k) && k.key_mouse.gamepad;
        if ((gamepad && gamepad.gamepad_exests) || ++waits > 240) clearInterval(waitForEngine);
    }, 250);

    window.__vndegen.onGamepad = (action, pressed, name) => {
        announcePad();
        if (action && !gameBinds(name)) window.__vndegenTyrano.button(action, pressed);
    };

    // A real or overlay tap means the player stopped using the controller focus.
    document.addEventListener('mousedown', () => { if (!firing) setFocus(null); }, true);

    const style = document.createElement('style');
    // Games with their own on image for these buttons mark them button-auto-sync or button-skip-sync.
    const modeLit = (mode) => `.vndegen-${mode}-on [data-event-pm*='"role":"${mode}"']:not(.button-${mode}-sync)`;
    style.textContent = '.vndegen-focus { outline: 3px solid #fff !important; outline-offset: 2px; ' +
        'box-shadow: 0 0 0 6px rgba(64, 156, 255, 0.85) !important; } ' +
        `${modeLit('skip')}, ${modeLit('auto')} { filter: drop-shadow(0 0 2px #fff) drop-shadow(0 0 5px rgb(30, 144, 255)) drop-shadow(0 0 10px rgb(30, 144, 255)); }`;
    document.addEventListener('DOMContentLoaded', () => document.head.appendChild(style));
})();
