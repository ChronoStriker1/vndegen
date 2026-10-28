import CNScripter
import Foundation
import Testing
import VNCore
@testable import EngineNScripter

/// An in-memory listing of `files` plus the folders above them, as `FolderProbe(scanning:)` would list them.
private func probe(_ files: [String]) -> FolderProbe {
    var entries = Set<String>()
    for file in files {
        var parts = file.split(separator: "/").map(String.init)
        while !parts.isEmpty {
            entries.insert(parts.joined(separator: "/"))
            parts.removeLast()
        }
    }
    return FolderProbe(root: URL(fileURLWithPath: "/nonexistent"), entries: entries)
}

@Test func encodedScriptsAreNScripter() {
    let match = NScripterPlugin().detect(probe(["Game/nscript.dat", "Game/arc.nsa"]))
    #expect(match?.confidence == .certain)
    #expect(match?.evidence == "Game/nscript.dat")
}

@Test func textScriptsNeedAnArchiveOrTheEnginesFiles() {
    #expect(NScripterPlugin().detect(probe(["0.txt", "readme.txt"])) == nil)
    #expect(NScripterPlugin().detect(probe(["00.txt", "arc.nsa"]))?.evidence == "00.txt")
    #expect(NScripterPlugin().detect(probe(["A/B/0.txt", "A/B/00.ns2"]))?.evidence == "A/B/0.txt")
    // Names compare without regard to case, as on Windows.
    #expect(NScripterPlugin().detect(probe(["Game/00.TXT", "Game/ARC.NSA"]))?.confidence == .certain)
}

@Test func theShallowestScriptFolderWins() throws {
    let layout = try #require(NScripterLayout(probe(["Game/nscript.dat", "Game/mod/Other/nscript.dat"])))
    #expect(layout.root == "Game")
}

@Test func nScripterGamesStartWithTheKeyboardAndMouseLayout() {
    #expect(NScripterPlugin().defaultControls == .keyboardAndMouse)
}

@Test func buttonsHoldKeysUntilTheLastRelease() {
    var router = NScripterInputRouter()
    let ret = KeyboardKey(key: "Enter", code: "Enter", keyCode: 13)
    #expect(router.translate(.button(.confirm, pressed: true)) { $0 } == [.key(Int32(VNNS_KEY_RETURN), down: true)])
    #expect(router.translate(.key(ret, pressed: true)) { $0 } == [])
    #expect(router.translate(.button(.confirm, pressed: false)) { $0 } == [])
    #expect(router.translate(.key(ret, pressed: false)) { $0 } == [.key(Int32(VNNS_KEY_RETURN), down: false)])
}

@Test func aRepeatingKeyIsReleasedByOneKeyUp() {
    var router = NScripterInputRouter()
    let returnKey = 40  // HID usage
    #expect(router.hostKey(hidUsage: returnKey, pressed: true) == [.key(Int32(VNNS_KEY_RETURN), down: true)])
    #expect(router.hostKey(hidUsage: returnKey, pressed: true) == [])
    #expect(router.hostKey(hidUsage: returnKey, pressed: false) == [.key(Int32(VNNS_KEY_RETURN), down: false)])
    #expect(router.hostKey(hidUsage: returnKey, pressed: true) == [.key(Int32(VNNS_KEY_RETURN), down: true)])
}

@Test func scrollingTowardThePlayerTurnsTheWheelDown() {
    var router = NScripterInputRouter()
    #expect(router.translate(.scroll(dy: NScripterInputRouter.pointsPerNotch)) { $0 } == [.wheel(-1)])
}

/// Runs a small script on the runtime and reads its frame back.
@Test func theRuntimeDrawsAScriptsBackground() throws {
    let dir = FileManager.default.temporaryDirectory.appendingPathComponent("vnns-\(UUID().uuidString)")
    try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    defer { try? FileManager.default.removeItem(at: dir) }
    let script = ";mode400\n*define\ngame\n*start\nbg #FF8000,1\nclick\nend\n"
    try Data(script.utf8).write(to: dir.appendingPathComponent("0.txt"))
    let engine = try #require(dir.path.withCString { root in
        var launch = VNNSLaunch(gameRoot: root, logPath: nil, englishWordWrap: false)
        return vnns_start(&launch)
    })
    defer { vnns_stop(engine) }
    #expect(vnns_step(engine, 16))
    var w: Int32 = 0, h: Int32 = 0
    var changed = false
    let pixels = try #require(vnns_frame(engine, &w, &h, &changed))
    #expect(w == 400 && h == 300)
    #expect(Array(UnsafeBufferPointer(start: pixels, count: 4)) == [0xFF, 0x80, 0x00, 0xFF])
}

/// Fixtures/spb/arc.nsa holds synthetic bitmaps packed by NScripter's own nsaarc.exe, which stores bitmaps with its
/// SPB compression. c_grad.bmp is 16 x 4, pixel (x, y) = (16x, 60y, 8(x + y)).
@Test func spbBitmapsDecodeExactly() throws {
    let dir = FileManager.default.temporaryDirectory.appendingPathComponent("vnns-\(UUID().uuidString)")
    try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    defer { try? FileManager.default.removeItem(at: dir) }
    let fixture = URL(fileURLWithPath: #filePath).deletingLastPathComponent().appendingPathComponent("Fixtures/spb/arc.nsa")
    try FileManager.default.copyItem(at: fixture, to: dir.appendingPathComponent("arc.nsa"))
    try Data("*define\nnsa\ngame\n*start\nbg \"c_grad.bmp\",1\nclick\nend\n".utf8).write(to: dir.appendingPathComponent("0.txt"))
    let engine = try #require(dir.path.withCString { root in
        var launch = VNNSLaunch(gameRoot: root, logPath: nil, englishWordWrap: false)
        return vnns_start(&launch)
    })
    defer { vnns_stop(engine) }
    #expect(vnns_step(engine, 16))
    var w: Int32 = 0, h: Int32 = 0
    var changed = false
    let pixels = try #require(vnns_frame(engine, &w, &h, &changed))
    // A background smaller than the 640 x 480 screen is centred.
    let left = (Int(w) - 16) / 2, top = (Int(h) - 4) / 2
    for (x, y) in [(0, 0), (5, 2), (15, 3)] {
        let i = ((top + y) * Int(w) + left + x) * 4
        #expect([pixels[i], pixels[i + 1], pixels[i + 2]] == [UInt8(16 * x), UInt8(60 * y), UInt8(8 * (x + y))])
    }
}

/// A 24-bit BMP of one colour.
private func solidBMP(width: Int, height: Int, rgb: (UInt8, UInt8, UInt8)) -> Data {
    let stride = (width * 3 + 3) & ~3
    var d = Data("BM".utf8)
    func le(_ v: Int, _ n: Int) { for i in 0..<n { d.append(UInt8(truncatingIfNeeded: v >> (8 * i))) } }
    le(54 + stride * height, 4); le(0, 4); le(54, 4)
    le(40, 4); le(width, 4); le(height, 4); le(1, 2); le(24, 2); le(0, 4); le(stride * height, 4); le(0, 16)
    let row = (0..<width).flatMap { _ in [rgb.2, rgb.1, rgb.0] } + Array(repeating: 0, count: stride - width * 3)
    for _ in 0..<height { d.append(contentsOf: row) }
    return d
}

/// Starts a script in a fresh folder holding `files`.
private func runScript(_ script: String, files: [String: Data] = [:]) throws -> (OpaquePointer, URL) {
    let dir = FileManager.default.temporaryDirectory.appendingPathComponent("vnns-\(UUID().uuidString)")
    try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    try Data(script.utf8).write(to: dir.appendingPathComponent("0.txt"))
    for (name, data) in files { try data.write(to: dir.appendingPathComponent(name)) }
    let engine = try #require(dir.path.withCString { root in
        var launch = VNNSLaunch(gameRoot: root, logPath: nil, englishWordWrap: false)
        return vnns_start(&launch)
    })
    return (engine, dir)
}

/// breakup.dll's pieces leave the sprite's place and fly up: a red square on white, broken with "urb", has red
/// above where it stood partway through.
@Test func breakupPiecesFlyOff() throws {
    let script = ";mode400\n*define\ngame\n*start\nbg white,1\nlsp 1,\":c;sq.bmp\",150,150\nprint 1\nwait 100\n" +
        "csp 1\nprint 99,1000,\"system\\breakup.dll/urb\"\nclick\nend\n"
    let (engine, dir) = try runScript(script, files: ["sq.bmp": solidBMP(width: 48, height: 48, rgb: (255, 0, 0))])
    defer { vnns_stop(engine); try? FileManager.default.removeItem(at: dir) }
    for _ in 0..<8 { #expect(vnns_step(engine, 16)) }  // through the wait; the effect starts
    for _ in 0..<12 { #expect(vnns_step(engine, 16)) }  // about a fifth of the way through
    var w: Int32 = 0, h: Int32 = 0
    var changed = false
    let pixels = try #require(vnns_frame(engine, &w, &h, &changed))
    func isRed(_ x: Int, _ y: Int) -> Bool {
        let i = (y * Int(w) + x) * 4
        return pixels[i] > 200 && pixels[i + 1] < 60 && pixels[i + 2] < 60
    }
    #expect((0..<150).contains { y in (100..<300).contains { x in isRed(x, y) } })
    // And the effect ends on the new frame: white everywhere.
    for _ in 0..<60 { #expect(vnns_step(engine, 16)) }
    let done = try #require(vnns_frame(engine, &w, &h, &changed))
    #expect(!(0..<Int(w * h)).contains { done[$0 * 4 + 1] < 200 })
}

/// Music in MP3 and Ogg plays and loops, and stopping it leaves nothing of it in the mix.
@Test(arguments: ["tone.mp3", "tone.ogg"]) func musicPlaysLoopsAndStops(file: String) throws {
    let fixture = URL(fileURLWithPath: #filePath).deletingLastPathComponent().appendingPathComponent("Fixtures/audio/\(file)")
    let script = "*define\ngame\n*start\nmp3loop \"\(file)\"\nwait 2000\nbgmstop\nclick\nend\n"
    let (engine, dir) = try runScript(script, files: [file: try Data(contentsOf: fixture)])
    defer { vnns_stop(engine); try? FileManager.default.removeItem(at: dir) }
    func level(_ frames: Int) -> Double {
        var buf = [Float](repeating: 0, count: frames * 2)
        vnns_audio_render(engine, &buf, Int32(frames))
        return (buf.reduce(0) { $0 + Double($1 * $1) } / Double(buf.count)).squareRoot()
    }
    // The decoder runs on its own thread, a little ahead of the mix.
    #expect(vnns_step(engine, 16))
    Thread.sleep(forTimeInterval: 0.05)
    var loud = true
    for _ in 0..<30 {  // 1.5 s of a 0.5 s file: across the loop point twice
        loud = loud && level(2205) > 0.05
        Thread.sleep(forTimeInterval: 0.02)
    }
    #expect(loud)
    for _ in 0..<3 { #expect(vnns_step(engine, 800)) }
    Thread.sleep(forTimeInterval: 0.05)
    #expect(level(4410) == 0)
}

@Test func savesGoToTheScriptsSaveFolder() throws {
    let dir = FileManager.default.temporaryDirectory.appendingPathComponent("vnns-\(UUID().uuidString)")
    let game = dir.appendingPathComponent("Game")
    try FileManager.default.createDirectory(at: game.appendingPathComponent("SAVE"), withIntermediateDirectories: true)
    defer { try? FileManager.default.removeItem(at: dir) }
    // nscript.dat is the script with each byte XOR 0x84.
    let script = ";mode800\r\n*define\r\n\tsavedir \"save\"\t;comment\r\ngame\r\n"
    try Data(script.utf8.map { $0 ^ 0x84 }).write(to: game.appendingPathComponent("nscript.dat"))
    let locations = NScripterPlugin().saveLocations(folder: dir, dataDirectory: dir)
    let saves = try #require(locations.first)
    #expect(locations.count == 1)
    // The folder on disk, whichever way the volume treats case.
    var isFolder: ObjCBool = false
    #expect(FileManager.default.fileExists(atPath: saves.directory.path, isDirectory: &isFolder) && isFolder.boolValue)
    #expect(saves.directory.lastPathComponent.lowercased() == "save")
    #expect(saves.relativePath("C:/Games/Game/save/save1.dat") == "save1.dat")
    #expect(saves.relativePath("C:/Games/Game/save/save1.bmp") == "save1.bmp")
    #expect(saves.relativePath(saves.directory.path + "/gloval.sav") == "gloval.sav")
    #expect(saves.relativePath("C:/Games/Game/save1.dat") == nil)
}

@Test func withoutASaveFolderOnlyTheEnginesFilesAreSaves() throws {
    let dir = FileManager.default.temporaryDirectory.appendingPathComponent("vnns-\(UUID().uuidString)")
    try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    defer { try? FileManager.default.removeItem(at: dir) }
    try Data("*define\ngame\n".utf8).write(to: dir.appendingPathComponent("0.txt"))
    try Data().write(to: dir.appendingPathComponent("arc.nsa"))
    let saves = try #require(NScripterPlugin().saveLocations(folder: dir, dataDirectory: dir).first)
    #expect(saves.directory.standardizedFileURL == dir.standardizedFileURL)
    for name in ["save12.dat", "gloval.sav", "kidoku.dat", "NScrflog.dat", "NScrllog.dat"] {
        #expect(saves.relativePath("D:/Game/\(name)") == name)
    }
    for name in ["nscript.dat", "save.dat", "arc.nsa", "0.txt"] {
        #expect(saves.relativePath("D:/Game/\(name)") == nil)
    }
}
