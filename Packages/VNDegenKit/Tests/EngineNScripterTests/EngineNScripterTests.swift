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
