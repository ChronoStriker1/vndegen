// Windows font substitutes for the page, injected at document start after the NW.js shim. Each Windows name in
// VNDegen's font table becomes a font face with that name, so CSS or a canvas that asks for "MS PGothic" gets Ume P
// Gothic. A font the device has under the Windows name, such as Arial on Apple systems, wins over the substitute.
// Faces load when the page first uses them. The ones the engine plugin found named in the game's files load now, and
// `__vndegen.fontsReady` turns true once they have, so an engine that draws text into canvases can wait for them.
(function () {
    'use strict';
    const config = __VNDEGEN_FONTS__;
    const loads = [];
    // Windows names a font's vertical variant with a leading @, which TyranoScript's CSS uses for vertical text.
    const faces = config.faces.flatMap((font) => [font, { ...font, name: '@' + font.name }]);
    for (const font of faces) {
        const source = 'local(' + JSON.stringify(font.name) + '), url(' + JSON.stringify(config.base + encodeURIComponent(font.file)) + ')';
        let face;
        try {
            face = new FontFace(font.name, source, { weight: font.bold ? 'bold' : 'normal', style: font.italic ? 'italic' : 'normal' });
        } catch (e) {
            continue;
        }
        document.fonts.add(face);
        if (config.preload.includes(font.name)) loads.push(face.load().catch((e) => console.warn('font ' + font.name + ': ' + e)));
    }
    window.__vndegen.fontsReady = loads.length === 0;
    if (loads.length) Promise.all(loads).then(() => { window.__vndegen.fontsReady = true; });
})();
