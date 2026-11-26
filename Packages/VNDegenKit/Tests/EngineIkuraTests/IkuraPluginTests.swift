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
        #expect(at(slot) == 33196)
        #expect(data.count == 0x500 + 33196)
        #expect(at(slot + 0x10c) == 112 && at(slot + 0x110) == 84)
        let state = slot + 0x6f54
        #expect(Array(data[state..<state + 9]) == Array("START.SNR".utf8))
        #expect(at(state + 12) == 0xffffffff)
        #expect(at(state + 0x2c) == 640)
        #expect(at(state + 0x80) == 512)
        #expect(at(state + 0x884) == 14)
        #expect(at(state + 0x930) == 29)
        #expect(at(state + 0x10e8) == 80)
        #expect(at(state + 0x1118) == 0xffffffff)
    }

    private var raidy2Sizes: [(UInt16, [UInt8])] {
        [(0x0300, Self.u16(900)), (0x0400, Self.u16(512)), (0x0c00, Self.u32(40) + Self.u32(40) + Self.u32(14)), (0x0100, Self.u16(27)),
         (0x0200, Self.u16(45)), (0x0700, Self.u32(80))]
    }

    private func savedState(_ ops: [(UInt16, [UInt8])]) throws -> (data: [UInt8], state: Int) {
        let start = Self.script(raidy2Sizes + ops + [
            (0x0a00, Self.name("TEST.SAV") + Self.u32(50)),
            (0x0a05, Self.u32(8) + Self.u32(112) + Self.u32(84)),
            (0x0a06, Self.u32(3)),
            (0x0706, []),
        ])
        let root = try makeGame(start)
        let engine = try #require(run(root, frames: 4))
        vnikura_close(engine)
        let data = try [UInt8](Data(contentsOf: root.appending(path: "TEST.SAV")))
        let slot = Self.read(data, 0x14 + 4 * 3)
        return (data, slot + 0x6f54)
    }

    static func read(_ data: [UInt8], _ o: Int) -> Int {
        Int(Int32(bitPattern: UInt32(data[o]) | UInt32(data[o + 1]) << 8 | UInt32(data[o + 2]) << 16 | UInt32(data[o + 3]) << 24))
    }

    @Test func aRaidy2SaveHasTheAutomapAndWideButtons() throws {
        let (data, state) = try savedState([])
        func at(_ o: Int) -> Int { Self.read(data, state + o) }
        #expect(at(0x2c) == 900)
        #expect(at(0xa1) == 512)
        #expect(at(0x8a5) == 40 && at(0x8a9) == 40 && at(0x8ad) == 14 && at(0x8b1) == 2800)
        #expect(at(0x13a5) == 27)
        #expect(at(0x14ed) == 45)
        #expect(at(0x20e5) == 80)
    }

    @Test func swappedLayersKeepTheirPlaceInTheDrawingOrder() throws {
        let (data, state) = try savedState([(0x0212, Self.u32(1) + Self.u32(5) + Self.u32(0))])
        func at(_ o: Int) -> Int { Self.read(data, state + o) }
        let order: [Int] = (0..<7).map { at(0x14f1 + 4 * $0) }
        let expected: [Int] = [0, 5, 2, 3, 4, 1, 6]
        #expect(order == expected)
    }

    @Test func conditionCodesCompareAsThePCDoes() throws {
        var ops = raidy2Sizes + [(0x0a00, Self.name("TEST.SAV") + Self.u32(50))]
        var labels = [Int]()
        var k = 0
        for (lhs, rhs) in [(1, 2), (2, 2), (3, 2)] {
            for code in 0..<6 {
                ops.append((0x0401, Self.u32(100 + k) + Self.u32(0)))
                ops.append((0x0407, Self.u32(lhs) + [UInt8(code)] + Self.u32(rhs) + [0] + Self.u16(labels.count) + [0xff]))
                ops.append((0x0004, Self.u16(labels.count + 1)))
                labels += [ops.count, ops.count + 1]
                ops.append((0x0401, Self.u32(100 + k) + Self.u32(1)))
                k += 1
            }
        }
        ops += [(0x0a05, Self.u32(8) + Self.u32(112) + Self.u32(84)), (0x0a06, Self.u32(3)), (0x0706, [])]
        let root = try makeGame(Self.script(ops, labels: labels))
        let engine = try #require(run(root, frames: 4))
        vnikura_close(engine)
        let data = try [UInt8](Data(contentsOf: root.appending(path: "TEST.SAV")))
        let state = Self.read(data, 0x14 + 4 * 3) + 0x6f54
        let results = (0..<18).map { Self.read(data, state + 0xa5 + 4 * (100 + $0)) }
        let expected = [0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 1]
        #expect(results == expected)
    }

    @Test func aShownLineIsMarkedReadInTheSaveFile() throws {
        let text: [UInt8] = [0x03, 0x04, 0x00, 0xff] + Self.u16(1) + [0x41, 0x02, 0x00]
        let start = Self.script(raidy2Sizes + [
            (0x0a00, Self.name("TEST.SAV") + Self.u32(50)),
            (0x0a0b, []),
            (0x090a, Self.u32(2)),
            (0x0908, Self.u32(10) + text),
        ])
        let root = try makeGame(start)
        var file = Self.u32(50) + Self.u32(0xdc) + Self.u32(0x1dc) + Self.u32(0x25d)
        file += [UInt8](repeating: 0, count: 0x25d - file.count) + Self.u32(10) + Self.u32(1280) + [UInt8](repeating: 0, count: 1280)
        try Data(file).write(to: root.appending(path: "TEST.SAV"))
        let engine = try #require(run(root, frames: 4))
        vnikura_close(engine)
        let data = try [UInt8](Data(contentsOf: root.appending(path: "TEST.SAV")))
        let marks = Array(data[0x265..<0x265 + 1280])
        var expected = [UInt8](repeating: 0, count: 1280)
        expected[2 * 128 + 1] = 0x20
        #expect(marks == expected)
    }

    @Test func closingTheGameWritesWhatItsQuitScriptWrites() throws {
        let start = Self.script(raidySizes + [
            (0x0a00, Self.name("TEST.SAV") + Self.u32(50)),
            (0x0a04, []),
            (0x0301, Self.u32(513)),
            (0x0706, []),
        ])
        let close = Self.script([(0x0a03, Self.u32(512) + Self.u32(8) + Self.u32(496) + Self.u32(1)), (0x0000, [])])
        let root = TestFolders.root.appending(path: "ikura-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        try Self.archive([("START.SNR", start), ("CLOSE.SNR", close)]).write(to: root.appending(path: "SNR"))
        try Self.archive([("BLANK.GGP", [0])]).write(to: root.appending(path: "GRP"))
        let engine = try #require(run(root, frames: 4))
        vnikura_close(engine)
        let data = try [UInt8](Data(contentsOf: root.appending(path: "TEST.SAV")))
        let global = Array(data[0xdc..<0xdc + 21])
        let expected: [UInt8] = Self.u32(512) + Self.u32(8) + Self.u32(496) + Self.u32(1) + [0x02] + Self.u32(0)
        #expect(global == expected)
    }

    @Test func equipmentStatsMatchTheOriginal() throws {
        let (data, state) = try savedState([
            (0x0401, Self.u32(154) + Self.u32(7)), (0x0401, Self.u32(150) + Self.u32(3)), (0x0401, Self.u32(162) + Self.u32(3)),
            (0x0f03, []),
        ])
        func v(_ i: Int) -> Int { Self.read(data, state + 0xa5 + 4 * i) }
        let stats: [Int] = [v(166), v(167), v(168), v(169), v(170), v(171), v(173)]
        let expected: [Int] = [7, 58, 6, 28, 21, 6, -3]
        #expect(stats == expected)
    }

    static func png(_ w: Int, _ h: Int, rgb: (UInt8, UInt8, UInt8)) -> [UInt8] {
        let crcTable: [UInt32] = (0..<256).map { n in
            (0..<8).reduce(UInt32(n)) { c, _ in c & 1 == 1 ? 0xedb8_8320 ^ (c >> 1) : c >> 1 }
        }
        func be(_ v: Int) -> [UInt8] { withUnsafeBytes(of: UInt32(truncatingIfNeeded: v).bigEndian) { Array($0) } }
        func chunk(_ type: String, _ body: [UInt8]) -> [UInt8] {
            let tagged = Array(type.utf8) + body
            let crc = tagged.reduce(UInt32(0xffff_ffff)) { crcTable[Int(($0 ^ UInt32($1)) & 0xff)] ^ ($0 >> 8) } ^ 0xffff_ffff
            return be(body.count) + tagged + be(Int(crc))
        }
        let pixel: [UInt8] = [rgb.0, rgb.1, rgb.2, 255]
        let row: [UInt8] = [0] + Array(repeating: pixel, count: w).joined()
        let raw: [UInt8] = Array(Array(repeating: row, count: h).joined())
        var a = 1, b = 0
        for x in raw {
            a = (a + Int(x)) % 65521
            b = (b + a) % 65521
        }
        let zlib: [UInt8] = [0x78, 0x01, 0x01] + u16(raw.count) + u16(~raw.count & 0xffff) + raw + be(b << 16 | a)
        return [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a] + chunk("IHDR", be(w) + be(h) + [8, 6, 0, 0, 0])
            + chunk("IDAT", zlib) + chunk("IEND", [])
    }

    @Test func aLoadedLayerKeepsThePictureItWasBuiltFrom() throws {
        let desert = Self.png(8, 8, rgb: (0x20, 0x80, 0x40)), logo = Self.png(8, 8, rgb: (0xc0, 0x10, 0x10))
        let play = Self.script(raidy2Sizes + [
            (0x0a00, Self.name("TEST.SAV") + Self.u32(50)),
            (0x0101, Self.u16(8) + Array("DESERT".utf8) + [0]),
            (0x0202, Self.u32(1) + Self.u32(8) + Self.u32(8)),
            (0x0207, Self.u32(1) + Self.u32(8) + Self.u32(0) + Self.u32(0)),
            (0x0208, Self.u32(1)),
            (0x0205, Self.u32(1) + Self.u32(1)),
            (0x0101, Self.u16(8) + Array("LOGO".utf8) + [0]),
            (0x0a05, Self.u32(20) + Self.u32(112) + Self.u32(84)),
            (0x0a06, Self.u32(3)),
            (0x0706, []),
        ], labels: [raidy2Sizes.count])
        let root = TestFolders.root.appending(path: "ikura-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        try Self.archive([("START.SNR", play)]).write(to: root.appending(path: "SNR"))
        try Self.archive([("DESERT", desert), ("LOGO", logo)]).write(to: root.appending(path: "GRP"))
        var w: Int32 = 0, h: Int32 = 0
        let live = try #require(run(root, frames: 4))
        let livePixel = try #require(vnikura_pixels(live, &w, &h))[4 * 800 + 4]
        vnikura_close(live)
        #expect(livePixel == 0xff40_8020)
        let load = Self.script(raidy2Sizes + [
            (0x0a00, Self.name("TEST.SAV") + Self.u32(50)),
            (0x0a07, Self.u32(3)),
            (0x0706, []),
        ], labels: [raidy2Sizes.count + 2])
        try Self.archive([("START.SNR", load), ("LOAD.SNR", Self.script([(0x0003, [])]))]).write(to: root.appending(path: "SNR"))
        let loaded = try #require(run(root, frames: 4))
        defer { vnikura_close(loaded) }
        let loadedPixel = try #require(vnikura_pixels(loaded, &w, &h))[4 * 800 + 4]
        #expect(loadedPixel == livePixel)
    }

    @Test func anAnimationFrameDrawsOnLayerFive() throws {
        var gan = Array("GANM0100".utf8) + Self.u32(3) + Self.u32(1)
        gan += Self.u32(1) + Self.u32(1) + Self.u32(2) + Self.u32(25) + Self.u32(5) + Self.u32(0)
        gan += [UInt8](repeating: 0, count: 0x2010 - gan.count)
        let frame: [UInt8] = [0x11, 0x22, 0x33, 0x11, 0x22, 0x33, 0xff, 0xff] + Self.u32(480000)
        gan += Self.u32(1) + Self.u32(0) + Self.u32(0x2020) + Self.u32(frame.count)
        gan += frame
        let start = Self.script(raidySizes + [
            (0x0202, Self.u32(5) + Self.u32(800) + Self.u32(600)),
            (0x0206, Self.u32(5) + Self.u32(255) + Self.u32(0) + Self.u32(0) + Self.u32(0)),
            (0x0208, Self.u32(5)),
            (0x0205, Self.u32(5) + Self.u32(1)),
            (0x0d00, Self.name("T")),
            (0x0d01, Self.u32(5)),
            (0x0706, []),
        ])
        let root = TestFolders.root.appending(path: "ikura-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        try Self.archive([("START.SNR", start)]).write(to: root.appending(path: "SNR"))
        try Self.archive([("T.GAN", gan)]).write(to: root.appending(path: "GRP"))
        let engine = try #require(run(root, frames: 4))
        defer { vnikura_close(engine) }
        var w: Int32 = 0, h: Int32 = 0
        let pixels = try #require(vnikura_pixels(engine, &w, &h))
        let colour: UInt32 = 0xff11_2233
        #expect(pixels[300 * 800 + 400] == colour)
        #expect(pixels[599 * 800 + 799] == colour)
    }

    static func hex(_ s: String) -> [UInt8] {
        let digits = Array(s)
        return stride(from: 0, to: digits.count, by: 2).map { UInt8(String(digits[$0...$0 + 1]), radix: 16) ?? 0 }
    }

    static func state(_ runs: String, count: Int) -> [UInt8] {
        var out = [UInt8](repeating: 0, count: count)
        for line in runs.split(separator: "\n") {
            let parts = line.split(separator: " ")
            guard parts.count == 2, let at = Int(parts[0]) else { continue }
            for (k, byte) in hex(String(parts[1])).enumerated() { out[at + k] = byte }
        }
        return out
    }

    static func message(_ line: Int, _ text: String) -> [UInt8] {
        u32(line) + [0x03, 0x04, 0xff, 0xff] + u16(text.utf8.count) + Array(text.utf8) + [0x02, 0x00]
    }

    private func click(_ engine: OpaquePointer, _ button: Int32) {
        vnikura_mouse_move(engine, 400, 300)
        _ = vnikura_frame(engine, 16)
        vnikura_mouse_button(engine, button, true)
        _ = vnikura_frame(engine, 16)
        vnikura_mouse_button(engine, button, false)
        for _ in 0..<60 { _ = vnikura_frame(engine, 16) }
    }

    private func slotMadeByRightClick(_ files: [String: [(String, [UInt8])]], slot: Int, clicks: Int) throws -> [UInt8] {
        let root = TestFolders.root.appending(path: "ikura-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        for (kind, entries) in files { try Self.archive(entries).write(to: root.appending(path: kind)) }
        let engine = try #require(run(root, frames: 60))
        for _ in 0..<clicks { click(engine, Int32(VNIKURA_MOUSE_LEFT)) }
        click(engine, Int32(VNIKURA_MOUSE_RIGHT))
        vnikura_close(engine)
        let data = try [UInt8](Data(contentsOf: root.appending(path: "TEST.SAV")))
        let at = Self.read(data, 0x14 + 4 * slot)
        return Array(data[at..<at + Self.read(data, at)])
    }

    @Test func aRaidy2SlotHoldsTheBytesThePCWroteAtTheSamePoint() throws {
        let picture = Self.png(8, 8, rgb: (0x40, 0x40, 0x40))
        var ops: [(UInt16, [UInt8])] = raidy2Sizes + [
            (0x0909, Self.u16(200)), (0x090c, Self.u16(100)), (0x0b00, Self.u32(16)),
            (0x0a00, Self.name("TEST.SAV") + Self.u32(50)),
            (0x0702, Self.u32(0x7fff_ffff) + Self.u16(3) + Self.u32(1)),
            (0x0101, Self.u16(1) + Array("M_WIN.GGP".utf8) + [0]), (0x0101, Self.u16(3) + Array("M_KEY.GGP".utf8) + [0]),
            (0x0101, Self.u16(4) + Array("FW.GGP".utf8) + [0]),
            (0x020a, Self.hex("000c00826c8272835383568362834e1600000002000000010000000000000000000000")),
            (0x0202, Self.u32(6) + Self.u32(800) + Self.u32(136)), (0x0203, Self.u32(6) + Self.u32(0) + Self.u32(600)),
            (0x0207, Self.u32(6) + Self.u32(1) + Self.u32(0) + Self.u32(0)), (0x0208, Self.u32(6)), (0x0205, Self.u32(6) + Self.u32(0)),
            (0x0900, Self.hex("060000006600000028000000b8020000760000001a000000")),
            (0x0901, Self.hex("0600000000000000000000005401000028000000040000002e000000")),
            (0x0902, Self.hex("000000000000000028000000")),
            (0x0903, Self.hex("06000000c402000060000000e4020000800000000300000004000000")),
            (0x0904, Self.hex("000000002000000000000000")),
            (0x090f, Self.hex("00000000ff000000ff000000ff000000ff000000c0000000180000001000000000000000")),
            (0x090a, Self.u32(7)),
            (0x0401, Self.u32(0) + Self.u32(1)), (0x0600, Self.name("RD2%02X") + Self.u32(1)),
            (0x0101, Self.u16(8) + Array("R2_001".utf8) + [0]), (0x0101, Self.u16(9) + Array("S2_001".utf8) + [0]),
            (0x0202, Self.u32(1) + Self.u32(800) + Self.u32(600)), (0x0207, Self.u32(1) + Self.u32(8) + Self.u32(0) + Self.u32(0)),
            (0x0208, Self.u32(1)), (0x0205, Self.u32(1) + Self.u32(1)),
            (0x0202, Self.u32(3) + Self.u32(400) + Self.u32(300)), (0x0203, Self.u32(3) + Self.u32(100) + Self.u32(50)),
            (0x0207, Self.u32(3) + Self.u32(9) + Self.u32(0) + Self.u32(0)), (0x0208, Self.u32(3)), (0x0205, Self.u32(3) + Self.u32(1)),
            (0x0202, Self.u32(5) + Self.u32(200) + Self.u32(100)),
            (0x0206, Self.u32(5) + Self.u32(255) + Self.u32(255) + Self.u32(0) + Self.u32(0)), (0x0208, Self.u32(5)),
            (0x0205, Self.u32(5) + Self.u32(0)),
            (0x0212, Self.u32(1) + Self.u32(3) + Self.u32(0)), (0x0212, Self.u32(3) + Self.u32(5) + Self.u32(0)),
            (0x090d, []), (0x0401, Self.u32(10) + Self.u32(3)),
        ]
        let line = ops.count
        ops += [(0x0908, Self.message(1, "Alpha")), (0x0706, []), (0x0000, [])]
        let save = ops.count
        ops += [(0x0a05, Self.u32(20) + Self.u32(112) + Self.u32(84)), (0x0a0c, Self.u32(2)), (0x0a06, Self.u32(0x8000_000a)),
                (0x0006, [])]
        let pictures = ["M_WIN.GGP", "M_KEY.GGP", "FW.GGP", "R2_001.GGP", "S2_001.GGP"].map { ($0, picture) }
        let slot = try slotMadeByRightClick(
            ["SNR": [("START.SNR", Self.script(ops, labels: [0, line, line, save]))], "GRP": pictures, "BGM": [("RD201.OGG", [0])]],
            slot: 3, clicks: 0)
        let pc = Self.state(Self.raidy2PCSlot, count: 8821)
        #expect(slot.count == 37321)
        #expect(Array(slot[0x0c..<0x10c]) == [UInt8](repeating: 0, count: 0x100))
        #expect(Array(slot[0x6f54...]) == pc)
    }

    @Test func aRaidySlotHoldsTheBytesThePCWroteAtTheSamePoint() throws {
        let picture = Self.png(8, 8, rgb: (0x40, 0x40, 0x40))
        var ops: [(UInt16, [UInt8])] = raidySizes + [
            (0x0909, Self.u16(1000)), (0x090c, Self.u16(100)), (0x0b00, Self.u32(16)),
            (0x0a00, Self.name("TEST.SAV") + Self.u32(50)),
            (0x0702, Self.u32(0x7fff_ffff) + Self.u16(3) + Self.u32(1)),
            (0x0101, Self.u16(0) + Array("DAN_P.GGP".utf8) + [0]), (0x0101, Self.u16(1) + Array("C_01_1.GGP".utf8) + [0]),
            (0x020a, Self.hex("000f0054696d6573204e657720526f6d616e1600000002000000010000000000000000000000")),
            (0x0202, Self.u32(2) + Self.u32(800) + Self.u32(136)), (0x0203, Self.u32(2) + Self.u32(0) + Self.u32(464)),
            (0x0207, Self.u32(2) + Self.u32(0) + Self.u32(0) + Self.u32(0x1d4)), (0x0208, Self.u32(2)), (0x0205, Self.u32(2) + Self.u32(0)),
            (0x0900, Self.hex("020000006600000028000000b8020000760000001a000000")),
            (0x0901, Self.hex("0200000000000000000000005401000028000000000000000e000000")),
            (0x0902, Self.hex("00000000f0000000e4010000")),
            (0x0903, Self.hex("02000000c402000060000000e4020000800000000000000004000000")),
            (0x0904, Self.hex("000000004001000024010000")),
            (0x090f, Self.hex("00000000ff000000ff000000ff000000ff000000c000000020000000120000000e000000")),
            (0x090a, Self.u32(7)),
            (0x0600, Self.name("RD01.OGG") + Self.u32(1)),
            (0x0202, Self.u32(1) + Self.u32(800) + Self.u32(600)), (0x0207, Self.u32(1) + Self.u32(1) + Self.u32(0) + Self.u32(0)),
            (0x0208, Self.u32(1)), (0x0205, Self.u32(1) + Self.u32(1)),
            (0x090d, []), (0x0401, Self.u32(10) + Self.u32(11)), (0x0004, Self.u16(1)), (0x0401, Self.u32(11) + Self.u32(99)),
        ]
        let line = ops.count
        ops += [(0x0908, Self.message(5, "M5")), (0x0908, Self.message(9, "LineM")), (0x0706, []), (0x0000, [])]
        let save = ops.count
        ops += [(0x0a05, Self.u32(9) + Self.u32(112) + Self.u32(84)), (0x0a0c, Self.u32(2)), (0x0a06, Self.u32(0x8000_000a)),
                (0x0006, [])]
        let pictures = ["DAN_P.GGP", "C_01_1.GGP"].map { ($0, picture) }
        let slot = try slotMadeByRightClick(
            ["SNR": [("START.SNR", Self.script(ops, labels: [0, line, line, save]))], "GRP": pictures, "BGM": [("RD01.OGG", [0])]],
            slot: 11, clicks: 1)
        let pc = Self.state(Self.raidyPCSlot, count: 4696)
        #expect(slot.count == 33196)
        #expect(Array(slot[0x0c..<0x11]) == Array("LineM".utf8) && slot[0x11] == 0)
        #expect(Array(slot[0x6f54...]) == pc)
    }

    static let raidy2PCSlot = """
        0 53544152542e534e520000000200000001000100070000000000000052443230312e4f4747000000010000008403
        162 02000001
        205 03
        2213 28000000280000000e000000f00a
        5029 1b0000000000000000000000000000004d5f57494e2e4747500000000000000000000000000000004d5f4b45592e4747
        5077 5000000046572e474750
        5129 52325f3030312e474750000053325f3030312e474750
        5357 2d000000000000000300000002000000050000000400000001000000060000000700000008000000090000000a000000
        5405 0b0000000c0000000d0000000e0000000f00000010000000110000001200000013000000140000001500000016000000
        5453 1700000018000000190000001a0000001b0000001c0000001d0000001e0000001f000000200000002100000022000000
        5501 230000002400000025000000260000002700000028000000290000002a0000002b0000002c00000006
        5589 ffffffff0000000000000000000000000700000020030000580200000000000000000000000048440000484400000000
        5637 000016440000000000001644ffffffff0800000000000000000000000000000006
        5717 ffffffff00000000000000000000000007000000900100002c0100000000c8420000c8420000fa430000fa4300004842
        5765 0000af43000048420000af43ffffffff0900000000000000000000000000000006
        5845 ffffffff00000000000000000000000006000000c8000000640000000000000000000000000048430000484300000000
        5893 0000c842000000000000c842ffffffffffffffff00000000000000000000ffff06000000200300008800000000000000
        5941 00000000000048440000484400001644000038440000164400003844ffffffff01000000000000000000000000000000
        5989 06
        6037 ffffffff00000000000000000000000006
        6101 ffffffff00000000000000000000000006
        6165 ffffffff00000000000000000000000006
        6229 ffffffff00000000000000000000000006
        6293 ffffffff00000000000000000000000006
        6357 ffffffff00000000000000000000000006
        6421 ffffffff00000000000000000000000006
        6485 ffffffff00000000000000000000000006
        6549 ffffffff00000000000000000000000006
        6613 ffffffff00000000000000000000000006
        6677 ffffffff00000000000000000000000006
        6741 ffffffff00000000000000000000000006
        6805 ffffffff00000000000000000000000006
        6869 ffffffff00000000000000000000000006
        6933 ffffffff00000000000000000000000006
        6997 ffffffff00000000000000000000000006
        7061 ffffffff00000000000000000000000006
        7125 ffffffff00000000000000000000000006
        7189 ffffffff00000000000000000000000006
        7253 ffffffff00000000000000000000000006
        7317 ffffffff00000000000000000000000006
        7381 ffffffff00000000000000000000000006
        7445 ffffffff00000000000000000000000006
        7509 ffffffff00000000000000000000000006
        7573 ffffffff00000000000000000000000006
        7637 ffffffff00000000000000000000000006
        7701 ffffffff00000000000000000000000006
        7765 ffffffff00000000000000000000000006
        7829 ffffffff00000000000000000000000006
        7893 ffffffff00000000000000000000000006
        7957 ffffffff00000000000000000000000006
        8021 ffffffff00000000000000000000000006
        8085 ffffffff00000000000000000000000006
        8149 ffffffff00000000000000000000000006
        8213 ffffffff00000000000000000000000006
        8277 ffffffff00000000000000000000000006
        8341 ffffffff00000000000000000000000006
        8405 ffffffff0000000000000000000000005000000053544152542e534e5200000003000000010000000000000000000000
        8453 00000000ffffffff00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
        8501 ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
        8549 ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
        8597 ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
        8645 ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
        8693 ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
        8741 ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
        """

    static let raidyPCSlot = """
        0 53544152542e534e5200000002000000010001000700000004000000524430312e4f474700000000010000008002
        129 02
        172 0b
        2180 0e00000044414e5f502e474750000000435f30315f312e474750
        2352 1d000000000000000100000002000000030000000400000005000000060000000700000008000000090000000a000000
        2400 0b0000000c0000000d0000000e0000000f00000010000000110000001200000013000000140000001500000016000000
        2448 1700000018000000190000001a0000001b0000001c00000006
        2520 ffffffff0000000000000000000000000700000020030000580200000000000000000000000048440000484400000000
        2568 000016440000000000001644ffffffff0100000000000000000000000000000006000000200300008800000000000000
        2616 0000000000004844000048440000e843000016440000e84300001644ffffffff0000000000000000d401000000000000
        2664 06
        2712 ffffffff00000000000000000000000006
        2776 ffffffff00000000000000000000000006
        2840 ffffffff00000000000000000000000006
        2904 ffffffff00000000000000000000000006
        2968 ffffffff00000000000000000000000006
        3032 ffffffff00000000000000000000000006
        3096 ffffffff00000000000000000000000006
        3160 ffffffff00000000000000000000000006
        3224 ffffffff00000000000000000000000006
        3288 ffffffff00000000000000000000000006
        3352 ffffffff00000000000000000000000006
        3416 ffffffff00000000000000000000000006
        3480 ffffffff00000000000000000000000006
        3544 ffffffff00000000000000000000000006
        3608 ffffffff00000000000000000000000006
        3672 ffffffff00000000000000000000000006
        3736 ffffffff00000000000000000000000006
        3800 ffffffff00000000000000000000000006
        3864 ffffffff00000000000000000000000006
        3928 ffffffff00000000000000000000000006
        3992 ffffffff00000000000000000000000006
        4056 ffffffff00000000000000000000000006
        4120 ffffffff00000000000000000000000006
        4184 ffffffff00000000000000000000000006
        4248 ffffffff00000000000000000000000006
        4312 ffffffff0000000000000000000000005000000053544152542e534e5200000003000000010000000000000000000000
        4360 00000000ffffffff00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
        4408 ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
        4456 ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
        4504 ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
        4552 ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
        4600 ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
        4648 ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
        """
}
