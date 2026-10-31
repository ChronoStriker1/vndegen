// Twine support. Injected at document start, after the NW.js shim.
// A Twine story is a page that scrolls, and a passage can run past the bottom of the screen. VNDegen's mouse wheel,
// which LB and RB turn in the "Keyboard and mouse" layout, arrives as a synthetic wheel event, which a browser never
// scrolls by itself. This scrolls the element under the pointer, or the first one above it that can scroll, by the
// wheel's distance.
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
