// Plays a Flash movie in Ruffle. The app opens flash.html?movie=<path>, where <path> is the movie's path in the game
// folder, such as /game.swf or /Game.exe for a projector, whose embedded movie the app serves at that path. Ruffle, its
// settings, and what ties its players to the session come from the user scripts flash-page.js and ruffle.js.
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

    window.__vndegenFlash = { player: player, ruffle: ruffle, movie: movie };
})();
