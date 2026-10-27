import FontPack
import Foundation
import OSLog
import Testing
import WebKit
@testable import WebRuntime

private let bundledFonts = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
    .appending(path: "../../../../Engines/Shared/fonts").standardizedFileURL

/// Configures the folders FontPackTests configures: FontPack's state is process-wide and both suites run at once, so
/// they must agree on it.
private func configureFonts() throws {
    let downloaded = FileManager.default.temporaryDirectory.appending(path: "vndegen-font-tests")
    try FileManager.default.createDirectory(at: downloaded, withIntermediateDirectories: true)
    for name in ["SourceHanSansKR-Regular.otf", "SourceHanSansKR-Bold.otf"] {
        try Data().write(to: downloaded.appending(path: name), options: .atomic)
    }
    #expect(FontPack.configure(folders: [bundledFonts, downloaded]))
}

/// The font substitutes in a page served over vndegen://.
@MainActor struct FontTests {
    private func page(preload: Set<String>) async throws -> PageHarness {
        try configureFonts()
        let root = FileManager.default.temporaryDirectory.appending(path: "vndegen-fonts-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        try Data("<html><body></body></html>".utf8).write(to: root.appending(path: "index.html"))
        let folder = GameFolder(root: root, webRoot: "")
        let page = try PageHarness(storage: false, scripts: [try WebGameSession.fontScript(preload: preload)],
                                   scheme: GameSchemeHandler(folder: folder, log: Logger())) { _ in .value(.null) }
        try await page.load(URLRequest(url: folder.indexURL))
        return page
    }

    @Test func preloadedSubstitutesDrawInCanvases() async throws {
        let page = try await page(preload: ["MS PGothic"])
        // 652.734 is "ABCあいう漢字" at 100 px in Ume P Gothic, from its advance widths.
        let width = try await page.run("""
            while (!window.__vndegen.fontsReady) await new Promise((done) => setTimeout(done, 20));
            const c = document.createElement('canvas').getContext('2d');
            c.font = '100px "MS PGothic"';
            return c.measureText('ABCあいう漢字').width;
            """) as? Double
        #expect(abs((width ?? 0) - 652.734) < 0.01)
    }

    @Test func everyNameHasAFaceAndAnAtFace() async throws {
        let page = try await page(preload: [])
        let families = try await page.run(#"return [...document.fonts].map((f) => f.family.replace(/^"(.*)"$/, '$1') + '/' + f.weight);"#) as? [String] ?? []
        for name in ["ＭＳ 明朝", "@ＭＳ 明朝", "Tahoma", "@Tahoma"] {
            #expect(families.contains(name + "/normal"), "\(name)")
        }
        #expect(families.contains("Tahoma/bold"))
        #expect(!families.contains { $0.hasPrefix("Microsoft YaHei/") })
    }

    @Test func servesOnlyTheTablesFonts() throws {
        try configureFonts()
        let font = GameSchemeHandler.serveFont(path: WebGameSession.fontPath + "ume-tgo4.ttf", method: "GET", range: "bytes=0-3")
        #expect(font?.status == 206)
        #expect(font?.headers["Content-Type"] == "font/ttf")
        #expect(GameSchemeHandler.serveFont(path: WebGameSession.fontPath + "fonts.tsv", method: "GET", range: nil)?.status == 404)
        #expect(GameSchemeHandler.serveFont(path: WebGameSession.fontPath + "../fonts.tsv", method: "GET", range: nil)?.status == 404)
        #expect(GameSchemeHandler.serveFont(path: "/js/plugins.js", method: "GET", range: nil) == nil)
    }
}
