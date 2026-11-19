import CAGE
import Foundation
import Testing
import TestSupport
import VNCore
@testable import EngineAGE

@Suite struct AGELayoutTests {
    private func probe(_ entries: [String]) -> FolderProbe { FolderProbe(root: URL(fileURLWithPath: "/game"), entries: Set(entries)) }

    @Test func findsTheCatalogBesideTheArchives() {
        let layout = AGELayout(probe(["age.exe", "SYS4INI.BIN", "DATA1.ALF", "APPEND01.AAI"]))
        #expect(layout?.root == "")
        #expect(layout?.evidence == "SYS4INI.BIN")
    }

    @Test func findsAGameOneFolderDownIgnoringCase() {
        let layout = AGELayout(probe(["Kamidori/sys4ini.bin", "Kamidori/Data1.alf"]))
        #expect(layout?.root == "Kamidori")
    }

    @Test func wantsAnArchiveOrTheEngine() {
        #expect(AGELayout(probe(["SYS4INI.BIN"])) == nil)
        #expect(AGELayout(probe(["a/b/SYS4INI.BIN", "a/b/DATA1.ALF"])) == nil)
    }
}

@Suite struct AGESaveTests {
    private let directory = URL(fileURLWithPath: "/data/saves")

    @Test func mapsWindowsAppDataPaths() {
        let root = "C:/Users/x/AppData/Local/Eushully/神採りアルケミーマイスター"
        #expect(AGEPlugin.saveName(root + "/SAVE/SAVE00.DAT", directory: directory) == "SAVE/SAVE00.DAT")
        #expect(AGEPlugin.saveName(root + "/SAVE/save12.sth", directory: directory) == "SAVE/save12.sth")
        #expect(AGEPlugin.saveName(root + "/SAVE/SAVE.DAT", directory: directory) == "SAVE/SAVE.DAT")
        #expect(AGEPlugin.saveName(root + "/SAVE/RT.DAT", directory: directory) == "SAVE/RT.DAT")
        #expect(AGEPlugin.saveName(root + "/SYS4REG.INI", directory: directory) == "SYS4REG.INI")
        #expect(AGEPlugin.saveName("C:\\Users\\x\\AppData\\Local\\Eushully\\Game\\SAVE\\SAVE03.DAT", directory: directory) == "SAVE/SAVE03.DAT")
    }

    @Test func mapsLocalPaths() {
        #expect(AGEPlugin.saveName("/data/saves/SAVE/SAVE01.DAT", directory: directory) == "SAVE/SAVE01.DAT")
        #expect(AGEPlugin.saveName("/data/saves/SYS4REG.INI", directory: directory) == "SYS4REG.INI")
    }

    @Test func refusesOtherFiles() {
        let root = "C:/Users/x/AppData/Local/Eushully/Game"
        #expect(AGEPlugin.saveName(root + "/SAVE/SAVE1.DAT", directory: directory) == nil)
        #expect(AGEPlugin.saveName(root + "/SAVE/notes.txt", directory: directory) == nil)
        #expect(AGEPlugin.saveName(root + "/$$AGE.TMP", directory: directory) == nil)
        #expect(AGEPlugin.saveName("C:/Games/Kamidori/DATA1.ALF", directory: directory) == nil)
        #expect(AGEPlugin.saveName("/data/saves/SAVE/../x.DAT", directory: directory) == nil)
    }
}

@Suite struct AGERuntimeTests {
    private func le(_ v: UInt32) -> [UInt8] { [UInt8(v & 255), UInt8(v >> 8 & 255), UInt8(v >> 16 & 255), UInt8(v >> 24)] }

    private func literals(_ data: [UInt8]) -> [UInt8] {
        var out: [UInt8] = []
        var i = 0
        while i < data.count {
            let chunk = data[i..<min(i + 8, data.count)]
            out.append(0xff)
            out += chunk
            i += 8
        }
        return out
    }

    private func makeGame(code: [UInt32]) throws -> URL {
        let root = TestFolders.root.appending(path: "age-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        var script = Array("SYS4433 ".utf8)
        let end = UInt32(code.count)
        for v: UInt32 in [4, 1, 1, 1, 1, 1, 0x1c, 0, end, 0, end, 0, end] { script += le(v) }
        for w in code { script += le(w) }
        try Data(script).write(to: root.appending(path: "DATA1.ALF"))
        var dir: [UInt8] = le(1)
        var arc = Array("DATA1.ALF".utf8)
        arc += [UInt8](repeating: 0, count: 0x100 - arc.count)
        dir += arc + le(1)
        var name = Array("SYSTEM4.BIN".utf8)
        name += [UInt8](repeating: 0, count: 0x40 - name.count)
        dir += name + le(0) + le(0) + le(0) + le(UInt32(script.count))
        let packed = literals(dir)
        var ini = [UInt8](repeating: 0, count: 0x138)
        ini.replaceSubrange(0..<8, with: Array("S4IC433 ".utf8))
        let id = Array("Test AGE".utf8)
        ini.replaceSubrange(8..<(8 + id.count), with: id)
        for (k, v) in [UInt32](arrayLiteral: 64, 1, 8, 1, 1, 1).enumerated() { ini.replaceSubrange((0xf0 + k * 4)..<(0xf4 + k * 4), with: le(v)) }
        for (k, v) in [UInt32](arrayLiteral: 64, 2, 8, 2, 2, 2).enumerated() { ini.replaceSubrange((0x114 + k * 4)..<(0x118 + k * 4), with: le(v)) }
        ini.replaceSubrange(0x130..<0x134, with: le(UInt32(dir.count)))
        ini.replaceSubrange(0x134..<0x138, with: le(UInt32(packed.count)))
        try Data(ini + packed).write(to: root.appending(path: "SYS4INI.BIN"))
        return root
    }

    private func open(_ root: URL, problem: inout [CChar]) -> OpaquePointer? {
        root.path.withCString { dir in
            var config = VNAgeConfig()
            config.gameDir = dir
            config.audioRate = 44100
            return vnage_open(&config, &problem, problem.count)
        }
    }

    @Test func probeReadsTheGameName() throws {
        let root = try makeGame(code: [0x2])
        var title = [CChar](repeating: 0, count: 256)
        #expect(vnage_probe(root.path, &title, title.count))
        #expect(String(decoding: title.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self) == "Test AGE")
    }

    @Test func runsTheRootScriptToItsEnd() throws {
        let root = try makeGame(code: [0x55, 3, 5, 0, 7, 0x50, 3, 6, 3, 5, 0, 3, 0x2])
        var problem = [CChar](repeating: 0, count: 256)
        let engine = try #require(open(root, problem: &problem))
        #expect(!vnage_frame(engine, 16))
        #expect(vnage_problem(engine) == nil)
        vnage_close(engine)
    }

    @Test func reportsAnUnknownOpcode() throws {
        let root = try makeGame(code: [0x3ff])
        var problem = [CChar](repeating: 0, count: 256)
        let engine = try #require(open(root, problem: &problem))
        #expect(!vnage_frame(engine, 16))
        let reason = vnage_problem(engine).map { String(cString: $0) } ?? ""
        #expect(reason.contains("unknown opcode 0x3ff"))
        vnage_close(engine)
    }

    @Test func openRefusesAFolderWithoutAGame() {
        var problem = [CChar](repeating: 0, count: 256)
        let engine = open(URL(fileURLWithPath: "/nonexistent"), problem: &problem)
        #expect(engine == nil)
    }
}
