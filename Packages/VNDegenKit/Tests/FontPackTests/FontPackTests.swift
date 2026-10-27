import Foundation
import Testing
@testable import FontPack

/// Runs against the real table and fonts in Engines/Shared/fonts. FontPack's state is process-wide, so the tests run
/// one at a time.
@Suite(.serialized)
struct FontPackTests {
    static let bundled = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
        .appending(path: "../../../../Engines/Shared/fonts").standardizedFileURL

    @Test func resolvesWindowsNames() {
        #expect(FontPack.configure(folders: [Self.bundled]))
        #expect(FontPack.resolve("MS PGothic")?.file.lastPathComponent == "ume-pgo4.ttf")
        #expect(FontPack.resolve("ＭＳ Ｐゴシック")?.file.lastPathComponent == "ume-pgo4.ttf")
        #expect(FontPack.resolve("ＭＳ 明朝")?.file.lastPathComponent == "ume-tmo3.ttf")
        let bold = FontPack.resolve("MS Gothic", bold: true)
        #expect(bold?.file.lastPathComponent == "ume-tgo4.ttf" && bold?.embolden == true)
        let arial = FontPack.resolve("arial", bold: true)
        #expect(arial?.file.lastPathComponent == "LiberationSans-Bold.ttf" && arial?.bold == true && arial?.embolden == false)
        #expect(FontPack.resolve("Comic Sans MS") == nil)
        #expect(FontPack.resolve("SimSun") == nil)
    }

    @Test func everyBundledFileIsInTheTable() throws {
        #expect(FontPack.configure(folders: [Self.bundled]))
        let used = Set(FontPack.entries.map(\.match.file.lastPathComponent))
        let files = try FileManager.default.contentsOfDirectory(atPath: Self.bundled.path).filter { $0.hasSuffix(".ttf") }
        #expect(Set(files) == used)
        #expect(FontPack.file(named: "ume-tgo4.ttf") != nil)
        #expect(FontPack.file(named: "fonts.tsv") == nil)
        #expect(FontPack.file(named: "../fonts.tsv") == nil)
    }

    @Test func downloadedFontsJoinTheTable() throws {
        let downloaded = FileManager.default.temporaryDirectory.appending(path: UUID().uuidString)
        try FileManager.default.createDirectory(at: downloaded, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: downloaded) }
        for name in ["SourceHanSansKR-Regular.otf", "SourceHanSansKR-Bold.otf"] {
            try Data().write(to: downloaded.appending(path: name))
        }
        #expect(FontPack.configure(folders: [Self.bundled, downloaded]))
        #expect(FontPack.resolve("굴림", bold: true)?.file.lastPathComponent == "SourceHanSansKR-Bold.otf")
        #expect(FontPack.folders == [Self.bundled, downloaded])
        #expect(FontPack.resolve("SimSun") == nil)
    }

    @Test func packagesExtractWhatTheTableNames() throws {
        let table = try String(contentsOf: Self.bundled.appending(path: "fonts.tsv"), encoding: .utf8)
        let named = Set(table.split(separator: "\n").filter { !$0.hasPrefix("#") }.map { $0.split(separator: "\t")[2] })
        for package in FontPack.packages {
            guard case .zip(let files) = package.installer else {
                Issue.record("\(package.id) is not a zip")
                continue
            }
            let fonts = files.map { ($0 as NSString).lastPathComponent }.filter { $0.hasSuffix(".otf") }
            #expect(!fonts.isEmpty && fonts.allSatisfy { named.contains(Substring($0)) })
        }
    }

    @Test func findsNamesInGameFiles() {
        #expect(FontPack.configure(folders: [Self.bundled]))
        let plugins = #"{"name":"MessageCore","parameters":{"Font Name":"ＭＳ　Ｐゴシック","Fallback":"arial, sans-serif"}}"#
        #expect(FontPack.referencedNames(in: plugins) == ["ＭＳ Ｐゴシック", "Arial"])
        #expect(FontPack.referencedNames(in: "GameFont, SimHei") == [])
        #expect(FontPack.referencedNames(in: "") == [])
    }

    @Test func missingTableLeavesNothing() {
        #expect(!FontPack.configure(folders: []))
        #expect(FontPack.entries.isEmpty)
        #expect(FontPack.resolve("Arial") == nil)
    }
}
