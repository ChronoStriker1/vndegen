// Plays a Flash movie in Ruffle. The app opens flash.html?movie=<path>, where <path> is the movie's path in the game
// folder, such as /game.swf or /Game.exe for a projector, whose embedded movie the app serves at that path. Ruffle and
// its settings come from the user scripts flash-page.js and ruffle.js.
(() => {
    'use strict';
    const params = new URLSearchParams(location.search);
    const movie = new URL(params.get('movie') || '/', location.origin).href;

    const player = window.RufflePlayer.newest().createPlayer();
    document.body.appendChild(player);
    const ruffle = player.ruffle();

    const options = {
        url: movie,
        // In a browser, Flash resolves the URLs a movie loads against the page. The PC projector resolves them against
        // the movie, and so does this.
        base: new URL('.', movie).href,
    };
    // For comparing Ruffle's renderers and reading the movie's trace() output: ?renderer=webgl&log=info.
    if (params.get('renderer')) options.preferredRenderer = params.get('renderer');
    if (params.get('log')) options.logLevel = params.get('log');
    // The generic fonts load first, for at most a few seconds, so the movie's first text is drawn in them.
    const fontsLoaded = Promise.race([window.__vndegenFlashFonts, new Promise((done) => setTimeout(done, 3000))]);
    fontsLoaded.then(() => ruffle.load(options))
        .then(() => player.focus(), (error) => console.error('flash: could not load ' + movie + ': ' + error));

    // A projector's Quit button sends fscommand("quit"), which ends the session as quitting the projector would. The
    // other commands set up the projector's window, which the session owns.
    ruffle.addFSCommandHandler((command) => {
        if (command.toLowerCase() === 'quit' && window.__vndegen) window.__vndegen.quit();
    });

    // The session's pointer, from the touch modes and controllers, arrives as synthetic mouse events on the element under
    // it, which is the player itself. Ruffle reads pointer events on its canvas in the player's shadow root, so they go
    // there as the pointer events a real mouse would make. Real mice and fingers reach the canvas themselves.
    const canvas = () => player.shadowRoot && player.shadowRoot.querySelector('canvas');
    const pointerTypes = { mousedown: 'pointerdown', mousemove: 'pointermove', mouseup: 'pointerup' };
    for (const type of Object.keys(pointerTypes)) {
        window.addEventListener(type, (event) => {
            const target = canvas();
            if (event.isTrusted || !target) return;
            event.stopImmediatePropagation();
            if (type === 'mousedown') player.focus();
            target.dispatchEvent(new PointerEvent(pointerTypes[type], {
                pointerId: 1, pointerType: 'mouse', isPrimary: true,
                clientX: event.clientX, clientY: event.clientY, screenX: event.screenX, screenY: event.screenY,
                button: event.button, buttons: event.buttons, bubbles: true, cancelable: true, composed: true, view: window,
            }));
        }, true);
    }
    window.addEventListener('wheel', (event) => {
        const target = canvas();
        if (event.isTrusted || !target || event.target === target) return;
        event.stopImmediatePropagation();
        target.dispatchEvent(new WheelEvent('wheel', {
            deltaX: event.deltaX, deltaY: event.deltaY, deltaMode: event.deltaMode, clientX: event.clientX, clientY: event.clientY,
            bubbles: true, cancelable: true, composed: true, view: window,
        }));
    }, true);

    if (window.__vndegen) {
        const pageOnPause = window.__vndegen.onPause;
        window.__vndegen.onPause = (paused) => {
            pageOnPause(paused);
            if (paused) {
                ruffle.suspend();
            } else {
                ruffle.resume();
                // Ruffle reads keys only while the player has focus, which the hub's menu takes away.
                player.focus();
            }
        };
    }

    window.__vndegenFlash = { player: player, ruffle: ruffle, movie: movie };
})();
