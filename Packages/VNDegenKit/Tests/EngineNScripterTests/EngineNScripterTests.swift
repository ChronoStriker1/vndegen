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
    #expect(saves.directory.lastPathComponent == "SAVE")
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
