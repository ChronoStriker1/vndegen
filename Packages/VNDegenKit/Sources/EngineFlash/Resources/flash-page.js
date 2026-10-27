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
        menu: false,
        showSwfDownload: false,
        warnOnUnsupportedContent: false,
        allowFullscreen: false,
        // Links to the developer's site open in the system browser through the session, never in the game's view.
        openUrlMode: 'deny',
        upgradeToHttps: false,
        // Device text in the browser's fonts, which cover Japanese where Ruffle's own fallback font does not.
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

    // Flash Player writes a movie's shared objects when it quits, and most games never ask for it sooner. Ruffle writes
    // them when the page hides, so a pagehide of our own writes them whenever the game may end: when it pauses, as when
    // the VNDegen menu opens or the app leaves the screen, when the page is hidden, and before the session quits.
    const flush = () => window.dispatchEvent(new PageTransitionEvent('pagehide', { persisted: true }));
    document.addEventListener('visibilitychange', () => { if (document.hidden) flush(); });
    if (window.__vndegen) {
        window.__vndegen.onPause = (paused) => { if (paused) flush(); };
        window.__vndegen.beforeQuit = flush;
    }

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
        location.href = '/__vndegen/flash.html?movie=' + encodeURIComponent(decodeURIComponent(url.pathname));
    }, true);
})();
