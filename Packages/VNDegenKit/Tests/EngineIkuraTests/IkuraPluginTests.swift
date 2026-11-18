import CIkura
import Foundation
import Testing
import TestSupport
import VNCore
@testable import EngineIkura

@Suite struct IkuraLayoutTests {
    private func probe(_ entries: [String]) -> FolderProbe { FolderProbe(root: URL(fileURLWithPath: "/game"), entries: Set(entries)) }

    @Test func findsTheArchivesAtTheRoot() {
        let layout = IkuraLayout(probe(["RaidyUS.exe", "SNR", "GRP", "SE", "BGM", "DATA", "VOICE", "RAIDYUS.SAV"]))
        #expect(layout?.root == "")
        #expect(layout?.evidence == "SNR")
    }

    @Test func findsTheArchivesInADiscFolder() {
        let layout = IkuraLayout(probe(["AUTORUN.INF", "Setup.exe", "RAIDY/snr", "RAIDY/Grp", "RAIDY/RaidyUS.exe"]))
        #expect(layout?.root == "RAIDY")
    }

    @Test func needsBothArchives() {
        #expect(IkuraLayout(probe(["SNR", "SE"])) == nil)
        #expect(IkuraLayout(probe(["SNR", "data/GRP"])) == nil)
        #expect(IkuraLayout(probe(["a/b/c/SNR", "a/b/c/GRP"])) == nil)
        #expect(IkuraLayout(probe(["SNR.dat", "GRP.dat"])) == nil)
    }
}

@Suite struct IkuraSaveTests {
    @Test func savesAreTheSavFilesInTheGameFolder() {
        #expect(IkuraPlugin.saveFileName("C:/Games/Raidy/RAIDYUS.SAV") == "RAIDYUS.SAV")
        #expect(IkuraPlugin.saveFileName("C:\\Games\\Raidy\\raidyus.sav") == "raidyus.sav")
        #expect(IkuraPlugin.saveFileName("C:/Games/Raidy/GRP") == nil)
        #expect(IkuraPlugin.saveFileName("C:/Games/Raidy/.sav") == nil)
    }
}

@Suite struct IkuraEngineTests {
    static func archive(_ files: [(String, [UInt8])]) -> Data {
        var out = Array("SM2MPX10".utf8)
        let start = 0x20 + files.count * 0x14
        func le(_ v: Int) -> [UInt8] { withUnsafeBytes(of: UInt32(v).littleEndian) { Array($0) } }
        out += le(files.count) + le(start)
        out += Array("SNR".utf8) + [UInt8](repeating: 0, count: 9) + le(0x20)
        var body = [UInt8]()
        for (name, bytes) in files {
            out += Array(name.utf8) + [UInt8](repeating: 0, count: 12 - name.utf8.count) + le(start + body.count) + le(bytes.count)
            body += bytes
        }
        return Data(out + body)
    }

    static func script(_ ops: [(UInt16, [UInt8])], labels: [Int] = []) -> [UInt8] {
        var code = [UInt8](), offsets = [Int]()
        for (op, args) in ops {
            offsets.append(code.count)
            code += withUnsafeBytes(of: op.littleEndian) { Array($0) }
            code += withUnsafeBytes(of: UInt16(args.count + 4).littleEndian) { Array($0) }
            code += args
        }
        func le(_ v: Int) -> [UInt8] { withUnsafeBytes(of: UInt32(v).littleEndian) { Array($0) } }
        let table = labels.flatMap { le(offsets[$0]) }
        return le(table.count) + le(code.count) + le(0) + le(0) + table + code
    }

    static func u16(_ v: Int) -> [UInt8] { withUnsafeBytes(of: UInt16(v).littleEndian) { Array($0) } }
    static func u32(_ v: Int) -> [UInt8] { withUnsafeBytes(of: UInt32(truncatingIfNeeded: v).littleEndian) { Array($0) } }
    static func name(_ s: String) -> [UInt8] { Array(s.utf8) + [UInt8](repeating: 0, count: 12 - s.utf8.count) }

    private func makeGame(_ start: [UInt8]) throws -> URL {
        let root = TestFolders.root.appending(path: "ikura-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        try Self.archive([("START.SNR", start)]).write(to: root.appending(path: "SNR"))
        try Self.archive([("BLANK.GGP", [0])]).write(to: root.appending(path: "GRP"))
        return root
    }

    private func run(_ root: URL, frames: Int) -> OpaquePointer? {
        var config = VNIkuraConfig()
        var problem = [CChar](repeating: 0, count: 256)
        let engine: OpaquePointer? = root.path.withCString { path in
            config.gameDir = path
            config.saveDir = path
            config.audioRate = 44100
            return vnikura_open(&config, &problem, problem.count)
        }
        if let engine {
            for _ in 0..<frames { _ = vnikura_frame(engine, 16) }
        }
        return engine
    }

    private var raidySizes: [(UInt16, [UInt8])] {
        [(0x0100, Self.u16(14)), (0x0200, Self.u16(29)), (0x0300, Self.u16(640)), (0x0400, Self.u16(512)), (0x0700, Self.u32(80))]
    }

    @Test func probeNeedsTheStartScript() throws {
        let root = try makeGame(Self.script([(0x0000, [])]))
        #expect(vnikura_probe(root.path, nil, 0))
        let other = TestFolders.root.appending(path: "ikura-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: other, withIntermediateDirectories: true)
        try Self.archive([("TITLE.SNR", Self.script([(0x0000, [])]))]).write(to: other.appending(path: "SNR"))
        try Self.archive([("BLANK.GGP", [0])]).write(to: other.appending(path: "GRP"))
        #expect(!vnikura_probe(other.path, nil, 0))
    }

    @Test func aFilledLayerShows() throws {
        let start = Self.script(raidySizes + [
            (0x0202, Self.u32(0) + Self.u32(800) + Self.u32(600)),
            (0x0206, Self.u32(0) + Self.u32(255) + Self.u32(255) + Self.u32(0) + Self.u32(0)),
            (0x0208, Self.u32(0)),
            (0x0205, Self.u32(0) + Self.u32(1)),
            (0x0706, []),
        ])
        let root = try makeGame(start)
        let engine = try #require(run(root, frames: 4))
        defer { vnikura_close(engine) }
        var w: Int32 = 0, h: Int32 = 0
        let pixels = try #require(vnikura_pixels(engine, &w, &h))
        #expect(w == 800 && h == 600)
        #expect(pixels[300 * 800 + 400] & 0xffffff == 0x0000ff)
    }

    @Test func aSaveHasThePCLayout() throws {
        let start = Self.script(raidySizes + [
            (0x0a00, Self.name("TEST.SAV") + Self.u32(50)),
            (0x0a05, Self.u32(8) + Self.u32(112) + Self.u32(84)),
            (0x0a06, Self.u32(3)),
            (0x0706, []),
        ])
        let root = try makeGame(start)
        let engine = try #require(run(root, frames: 2))
        vnikura_close(engine)
        let data = try [UInt8](Data(contentsOf: root.appending(path: "TEST.SAV")))
        func at(_ o: Int) -> Int { Int(data[o]) | Int(data[o + 1]) << 8 | Int(data[o + 2]) << 16 | Int(data[o + 3]) << 24 }
        #expect(at(0) == 50)
        #expect(at(8) == 0xdc && at(12) == 0x114)
        #expect(at(0x10) == 3)
        let slot = at(0x14 + 4 * 3)
        #expect(slot == 0x500)
        #expect(at(slot) == 37044)
        #expect(data.count == 0x500 + 37044)
        #expect(at(slot + 0x10c) == 112 && at(slot + 0x110) == 84)
        let state = slot + 0x6f54
        #expect(Array(data[state..<state + 9]) == Array("START.SNR".utf8))
        #expect(at(state + 12) == 0xffffffff)
        #expect(at(state + 0x2c) == 640)
        #expect(at(state + 0x80) == 512)
        #expect(at(state + 0x884) == 14)
        #expect(at(state + 0x930) == 29)
        #expect(at(state + 0x10e8) == 80)
    }
}
