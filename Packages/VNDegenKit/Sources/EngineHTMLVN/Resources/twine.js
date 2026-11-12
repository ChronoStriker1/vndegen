// Twine support. Injected at document start, after the NW.js shim.
(function () {
    'use strict';

    function scrollable(element) {
        for (let el = element; el && el !== document.documentElement; el = el.parentElement) {
            const style = getComputedStyle(el);
            if (/(auto|scroll)/.test(style.overflowY) && el.scrollHeight > el.clientHeight) return el;
        }
        return document.scrollingElement || document.documentElement;
    }

    window.addEventListener('wheel', (event) => {
        if (event.isTrusted || event.defaultPrevented) return;
        const lines = event.deltaMode === 1 ? 16 : event.deltaMode === 2 ? innerHeight : 1;
        scrollable(event.target).scrollBy({ top: event.deltaY * lines, left: event.deltaX * lines });
    });
})();
