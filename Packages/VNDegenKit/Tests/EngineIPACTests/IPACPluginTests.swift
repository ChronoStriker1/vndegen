import CIPAC
import Foundation
import Testing
import TestSupport
import VNCore
@testable import EngineIPAC

@Suite struct IPACLayoutTests {
    private func probe(_ entries: [String]) -> FolderProbe { FolderProbe(root: URL(fileURLWithPath: "/game"), entries: Set(entries)) }

    @Test func findsTheDataFolderBesideTheExe() {
        let layout = IPACLayout(probe(["Lamia_girl.exe", "DATA/Scr.PAK", "DATA/Parts.PAK", "DATA/BG.PAK", "DATA/BGM/BGM01.WST"]))
        #expect(layout?.root == "")
        #expect(layout?.evidence == "DATA/Scr.PAK")
    }

    @Test func findsAGameOneFolderDownIgnoringCase() {
        let layout = IPACLayout(probe(["01 Lamia/data/SCR.pak", "01 Lamia/data/parts.PAK", "01 Lamia/readme/Readme.html"]))
        #expect(layout?.root == "01 Lamia")
        #expect(layout?.evidence == "01 Lamia/data/SCR.pak")
    }

    @Test func wantsThePartsArchiveInTheSameDataFolder() {
        #expect(IPACLayout(probe(["DATA/Scr.PAK"])) == nil)
        #expect(IPACLayout(probe(["DATA/Scr.PAK", "Parts.PAK"])) == nil)
        #expect(IPACLayout(probe(["Scr.PAK", "Parts.PAK"])) == nil)
        #expect(IPACLayout(probe(["a/b/DATA/Scr.PAK", "a/b/DATA/Parts.PAK"])) == nil)
    }
}

@Suite struct IPACSaveTests {
    private let folder = IPACPlugin.SaveFolder(brand: "Ｖａｎａｄｉｓ", title: "魔物娘との性活～ラミアの場合～")
    private let data = URL(fileURLWithPath: "/data/game1")

    @Test func keepsSavesWhereThePCKeepsThemUnderDocuments() {
        let directory = IPACPlugin.saveDirectory(data, folder: folder)
        #expect(directory.path == "/data/game1/Documents/Ｖａｎａｄｉｓ/魔物娘との性活～ラミアの場合～")
    }

    @Test func mapsAWindowsBackupPathToTheSaveFile() {
        let directory = IPACPlugin.saveDirectory(data, folder: folder)
        let windows = #"C:\Users\player\Documents\Ｖａｎａｄｉｓ\魔物娘との性活～ラミアの場合～\_DAT0003.DAT"#
        #expect(IPACPlugin.saveRelativePath(windows, directory: directory, folder: folder) == "_DAT0003.DAT")
        let proton = "/home/deck/.wine/drive_c/users/steamuser/My Documents/Ｖａｎａｄｉｓ/魔物娘との性活～ラミアの場合～/_SYSDAT.DAT"
        #expect(IPACPlugin.saveRelativePath(proton, directory: directory, folder: folder) == "_SYSDAT.DAT")
    }

    @Test func mapsALocalSaveAndRejectsOtherFiles() {
        let directory = IPACPlugin.saveDirectory(data, folder: folder)
        #expect(IPACPlugin.saveRelativePath(directory.path + "/_DAT0099.DAT", directory: directory, folder: folder) == "_DAT0099.DAT")
        #expect(IPACPlugin.saveRelativePath("C:/Users/player/Documents/Ｖａｎａｄｉｓ/Other Game/_DAT0000.DAT", directory: directory, folder: folder) == nil)
        #expect(IPACPlugin.saveRelativePath("C:/Games/Lamia/DATA/Scr.PAK", directory: directory, folder: folder) == nil)
    }
}

@Suite struct IPACProbeTests {
    static func pak(_ files: [(String, [UInt8])]) -> Data {
        var index = [UInt8](), body = [UInt8]()
        let base = 8 + files.count * 0x2c
        for (name, bytes) in files {
            var entry = [UInt8](repeating: 0, count: 0x2c)
            let encoded = Array(name.data(using: .shiftJIS)!)
            entry.replaceSubrange(0..<encoded.count, with: encoded)
            withUnsafeBytes(of: UInt32(base + body.count).littleEndian) { entry.replaceSubrange(0x24..<0x28, with: $0) }
            withUnsafeBytes(of: UInt32(bytes.count).littleEndian) { entry.replaceSubrange(0x28..<0x2c, with: $0) }
            index += entry
            body += bytes
        }
        var header = Array("IPAC".utf8)
        withUnsafeBytes(of: UInt16(files.count).littleEndian) { header += $0 }
        header += [0, 0]
        return Data(header + index + body)
    }

    static func script(_ lines: [String]) -> [UInt8] {
        Array(lines.joined(separator: "\r\n").data(using: .shiftJIS)!)
    }

    private func makeGame(_ name: String, scripts: [(String, [UInt8])]) throws -> URL {
        let root = TestFolders.root.appending(path: "ipac-\(name)-\(UUID().uuidString)")
        let data = root.appending(path: "DATA")
        try FileManager.default.createDirectory(at: data, withIntermediateDirectories: true)
        try Self.pak(scripts).write(to: data.appending(path: "Scr.PAK"))
        try Self.pak([("blank.IES", [UInt8](repeating: 0, count: 32))]).write(to: data.appending(path: "Parts.PAK"))
        return root
    }

    @Test func probeReadsTheTitleFromTheGameInfoLine() throws {
        let root = try makeGame("probe", scripts: [
            ("seenINIT.org", Self.script(["#LABEL Test", "#GAMEINFO Ｖａｎａｄｉｓ テストの場合"])),
            ("TITLE.org", Self.script(["[黒バック]"])),
        ])
        var title = [CChar](repeating: 0, count: 256)
        #expect(vnipac_probe(root.path, &title, title.count))
        #expect(String(decoding: title.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self) == "テストの場合")
        let folder = IPACPlugin.saveFolder(root)
        #expect(folder == IPACPlugin.SaveFolder(brand: "Ｖａｎａｄｉｓ", title: "テストの場合"))
    }

    @Test func probeRefusesArchivesWithoutTheTitleScript() throws {
        let root = try makeGame("empty", scripts: [("seenINIT.org", Self.script(["#GAMEINFO Ｖａｎａｄｉｓ テスト"]))])
        var title = [CChar](repeating: 0, count: 256)
        #expect(!vnipac_probe(root.path, &title, title.count))
    }
}
