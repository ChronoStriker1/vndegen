import CTmrHiro
import Foundation
import Testing
import TestSupport
import VNCore
@testable import EngineTmrHiro

@Suite struct TmrHiroLayoutTests {
    private func probe(_ entries: [String]) -> FolderProbe { FolderProbe(root: URL(fileURLWithPath: "/game"), entries: Set(entries)) }

    @Test func findsTheArchivesBesideTheExe() {
        let layout = TmrHiroLayout(probe(["game.exe", "Agrd.pac", "Bgrd.pac", "srp.pac", "srp_2.pac", "save"]))
        #expect(layout?.root == "")
        #expect(layout?.evidence == "srp.pac")
    }

    @Test func findsAGameInADataFolderIgnoringCase() {
        let layout = TmrHiroLayout(probe(["manual/top.html", "DATA/AGRD.PAC", "DATA/Srp.pac", "extra/a/agrd.pac", "extra/a/srp.pac"]))
        #expect(layout?.root == "DATA")
        #expect(layout?.evidence == "DATA/Srp.pac")
    }

    @Test func wantsThePictureArchiveBesideTheScripts() {
        #expect(TmrHiroLayout(probe(["srp.pac"])) == nil)
        #expect(TmrHiroLayout(probe(["srp.pac", "data/Agrd.pac"])) == nil)
        #expect(TmrHiroLayout(probe(["a/b/c/srp.pac", "a/b/c/Agrd.pac"])) == nil)
    }
}

@Suite struct TmrHiroSaveTests {
    @Test func recognizesTheEngineSaveFiles() {
        #expect(TmrHiroPlugin.saveName("C:/Games/OIDEYO/save/savedata.000") == "savedata.000")
        #expect(TmrHiroPlugin.saveName("C:\\Games\\OIDEYO\\save\\savedata.101") == "savedata.101")
        #expect(TmrHiroPlugin.saveName("/x/save/savedata999.bmp") == "savedata999.bmp")
        #expect(TmrHiroPlugin.saveName("/x/save/savedata.1") == nil)
        #expect(TmrHiroPlugin.saveName("/x/save/ss/SS_000.bmp") == nil)
        #expect(TmrHiroPlugin.saveName("/x/srp.pac") == nil)
    }
}

@Suite struct TmrHiroRuntimeTests {
    private static func le<T: FixedWidthInteger>(_ v: T) -> [UInt8] { withUnsafeBytes(of: v.littleEndian, Array.init) }

    static func pac(_ entries: [(String, [UInt8])]) -> Data {
        let nameLength = 16
        let dataStart = 7 + (nameLength + 12) * entries.count
        var out: [UInt8] = le(UInt16(entries.count)) + [UInt8(nameLength)] + le(UInt32(dataStart))
        var offset: UInt64 = 0
        for (name, body) in entries {
            var n = Array(name.utf8)
            n += [UInt8](repeating: 0, count: nameLength - n.count)
            out += n + le(offset) + le(UInt32(body.count))
            offset += UInt64(body.count)
        }
        for (_, body) in entries { out += body }
        return Data(out)
    }

    static func ini(_ lines: [String]) -> [UInt8] {
        lines.flatMap { line -> [UInt8] in
            let bytes = Array(line.utf8)
            return le(UInt16(bytes.count)) + bytes
        }
    }

    @Test func probeReadsTheGameTitleFromTheSystemSettings() throws {
        let root = TestFolders.root.appending(path: "tmrhiro-probe-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        try Self.pac([("_system", Self.ini(["[GAMESYSTEM]", "GAME_TITLE=Test Park", "SAVE_TYPE=0"]))]).write(to: root.appending(path: "srp.pac"))
        try Self.pac([("logo", [UInt8](repeating: 0, count: 32))]).write(to: root.appending(path: "Agrd.pac"))
        var title = [CChar](repeating: 0, count: 256)
        #expect(vnth_probe(root.path, &title, title.count))
        #expect(String(decoding: title.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self) == "Test Park")
    }

    @Test func probeRefusesAFolderWithoutScripts() throws {
        let root = TestFolders.root.appending(path: "tmrhiro-empty-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        try Self.pac([("logo", [UInt8](repeating: 0, count: 32))]).write(to: root.appending(path: "Agrd.pac"))
        var title = [CChar](repeating: 0, count: 256)
        #expect(!vnth_probe(root.path, &title, title.count))
    }
}
