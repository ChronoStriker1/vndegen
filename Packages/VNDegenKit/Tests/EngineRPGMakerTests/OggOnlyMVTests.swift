import Foundation
import OSLog
import Testing
import WebKit
@testable import EngineRPGMaker
@testable import WebRuntime

private let tone = URL(filePath: #filePath).deletingLastPathComponent()
    .appending(path: "../WebRuntimeTests/Fixtures/tone.ogg").standardized

/// An MV game folder with www/js/rpg_core.js and the given files under www/.
private func mvGame(_ files: [String], core: String = "rpg_core.js") throws -> URL {
    let fm = FileManager.default
    let root = fm.temporaryDirectory.appending(path: "vndegen-mv-ogg-\(UUID().uuidString)", directoryHint: .isDirectory)
    for file in ["js/\(core)"] + files {
        let url = root.appending(path: "www/\(file)")
        try fm.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        if file.hasSuffix(".ogg") { try fm.copyItem(at: tone, to: url) } else { try Data().write(to: url) }
    }
    return root
}

@Test @MainActor func onlyMVGamesWithoutM4AGetThePageDecoder() throws {
    let cases: [([String], RPGMakerPlugin.Flavor, Bool)] = [
        (["audio/bgm/Theme.ogg", "audio/se/Cursor.ogg"], .mv, true),
        (["Audio/SE/Cursor.RPGMVO"], .mv, true),
        (["audio/bgm/Theme.ogg", "audio/bgm/Theme.m4a"], .mv, false),
        (["audio/se/Cursor.rpgmvo", "audio/se/Cursor.rpgmvm"], .mv, false),
        (["img/system/Window.png"], .mv, false),
        (["audio/bgm/Theme.ogg"], .mz, false),
    ]
    for (files, flavor, decodes) in cases {
        let root = try mvGame(files)
        defer { try? FileManager.default.removeItem(at: root) }
        let configuration = try RPGMakerPlugin(flavor).webConfiguration(GameFolder(rpgMakerGameAt: root))
        #expect(configuration.decodesOgg == decodes, "\(flavor) \(files)")
    }
}

/// MV's own codec check, request, and decode, as rpg_core.js 1.6 has them: it asks for .ogg when the page says it can
/// play Ogg, and hands the file's bytes to decodeAudioData with a success callback only.
private let mvAudio = """
    window.WebAudio = {
        ext: () => (document.createElement('audio').canPlayType('audio/ogg') ? '.ogg' : '.m4a'),
        load: (name) => new Promise((resolve) => {
            const xhr = new XMLHttpRequest();
            xhr.open('GET', 'audio/se/' + name + WebAudio.ext());
            xhr.responseType = 'arraybuffer';
            xhr.onload = () => {
                if (xhr.status >= 400) return resolve('HTTP ' + xhr.status);
                new OfflineAudioContext(2, 1, 22050).decodeAudioData(xhr.response, (b) => resolve(b.length + ' at ' + b.sampleRate));
                setTimeout(() => resolve('nothing'), 5000);
            };
            xhr.send();
        }),
    };
    """

/// Runs MV's audio code in a page with the RPG Maker additions, and with the shared Ogg decoder when the game ships no
/// .m4a. Scripts go in the order the session injects them.
@MainActor private func mvPage(_ root: URL) async throws -> WKWebView {
    try Data(#"<html><body><script src="js/rpg_core.js"></script></body></html>"#.utf8).write(to: root.appending(path: "www/index.html"))
    try Data(mvAudio.utf8).write(to: root.appending(path: "www/js/rpg_core.js"))
    let game = try RPGMakerPlugin(.mv).webConfiguration(GameFolder(rpgMakerGameAt: root))
    let configuration = WKWebViewConfiguration()
    configuration.setURLSchemeHandler(GameSchemeHandler(folder: game.folder, log: Logger()), forURLScheme: GameFolder.scheme)
    let ogg = game.decodesOgg ? [try WebGameSession.oggScript()] : []
    for script in ["window.__vndegen = {};"] + ogg + game.userScripts {
        configuration.userContentController.addUserScript(WKUserScript(source: script, injectionTime: .atDocumentStart, forMainFrameOnly: true))
    }
    let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
    webView.load(URLRequest(url: game.folder.indexURL))
    let deadline = ContinuousClock.now + .seconds(20)
    while try await webView.evaluateJavaScript("document.readyState === 'complete' && typeof WebAudio === 'object'") as? Bool != true {
        guard ContinuousClock.now < deadline else { throw CancellationError() }
        try await Task.sleep(for: .milliseconds(50))
    }
    return webView
}

/// An MV game that ships only Ogg asked for .m4a and played nothing. Now it asks for .ogg, and the page's decoder
/// decodes it at the file's own 44.1 kHz, where WebKit's would resample it to the context's 22.05 kHz. A game that
/// ships .m4a still asks for it.
@Test @MainActor func mvPlaysOggWhenItShipsNoM4A() async throws {
    let oggOnly = try mvGame(["audio/se/tone.ogg"])
    defer { try? FileManager.default.removeItem(at: oggOnly) }
    let page = try await mvPage(oggOnly)
    let decoded = try await page.callAsyncJavaScript("return [WebAudio.ext(), await WebAudio.load('tone')];",
                                                     arguments: [:], contentWorld: .page) as? [String]
    #expect(decoded == [".ogg", "44100 at 44100"])

    let both = try mvGame(["audio/se/tone.ogg", "audio/se/tone.m4a"])
    defer { try? FileManager.default.removeItem(at: both) }
    let asBefore = try await mvPage(both).callAsyncJavaScript("return WebAudio.ext();", arguments: [:], contentWorld: .page) as? String
    #expect(asBefore == ".m4a")
}
