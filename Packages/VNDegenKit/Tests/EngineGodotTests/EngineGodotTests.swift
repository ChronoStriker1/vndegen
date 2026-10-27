import Foundation
import Testing
import VNCore
@testable import EngineGodot

/// Builds a game folder from relative paths and file contents, and deletes it afterwards.
private final class GameFolder {
    let root: URL

    init(_ files: [String: Data]) throws {
        root = FileManager.default.temporaryDirectory.appendingPathComponent("godot-\(UUID().uuidString)", isDirectory: true)
        for (path, data) in files {
            let file = root.appendingPathComponent(path)
            try FileManager.default.createDirectory(at: file.deletingLastPathComponent(), withIntermediateDirectories: true)
            try data.write(to: file)
        }
    }

    deinit { try? FileManager.default.removeItem(at: root) }

    var probe: FolderProbe { FolderProbe(scanning: root) }
}

/// Little-endian bytes, as Godot writes its packs.
private struct Bytes {
    var data = Data()
    mutating func u32(_ value: UInt32) { withUnsafeBytes(of: value.littleEndian) { data.append(contentsOf: $0) } }
    mutating func u64(_ value: UInt64) { withUnsafeBytes(of: value.littleEndian) { data.append(contentsOf: $0) } }
    mutating func zeros(_ count: Int) { data.append(Data(count: count)) }
}

/// A pack as Godot exports it, with empty files. Format 1 is Godot 3's, 2 Godot 4.0 to 4.4's, 3 Godot 4.5's.
private func pack(format: UInt32, version: (UInt32, UInt32, UInt32), files: [String], flags: UInt32 = 0) -> Data {
    var directory = Bytes()
    directory.u32(UInt32(files.count))
    for file in files {
        var name = Data(file.utf8)
        name.append(Data(count: (4 - name.count % 4) % 4))
        directory.u32(UInt32(name.count))
        directory.data.append(name)
        directory.u64(0)
        directory.u64(0)
        directory.zeros(16)
        if format >= 2 { directory.u32(0) }
    }
    var bytes = Bytes()
    bytes.u32(GodotPack.magic)
    bytes.u32(format)
    bytes.u32(version.0)
    bytes.u32(version.1)
    bytes.u32(version.2)
    if format >= 2 {
        bytes.u32(flags)
        bytes.u64(0)
    }
    if format >= 3 {
        bytes.u64(UInt64(bytes.data.count + 8 + 64))
    }
    bytes.zeros(64)
    bytes.data.append(directory.data)
    return bytes.data
}

/// A Windows executable with `payload` in a section named "pck", followed by a signature.
private func executable(with payload: Data) -> Data {
    var bytes = Bytes()
    bytes.data.append(Data("MZ".utf8))
    bytes.zeros(0x3C - 2)
    bytes.u32(0x80) // PE header offset
    bytes.zeros(0x80 - bytes.data.count)
    bytes.u32(0x0000_4550) // "PE\0\0"
    bytes.data.append(contentsOf: [0x64, 0x86]) // machine
    bytes.data.append(contentsOf: [2, 0]) // two sections
    bytes.zeros(12)
    bytes.data.append(contentsOf: [0, 0]) // no optional header
    bytes.zeros(2)
    let sectionStart = 0x200
    for (name, offset) in [(".text", 0), ("pck", sectionStart)] {
        var field = Data(name.utf8)
        field.append(Data(count: 8 - field.count))
        bytes.data.append(field)
        bytes.zeros(12)
        bytes.u32(UInt32(offset)) // raw data offset
        bytes.zeros(16)
    }
    bytes.zeros(sectionStart - bytes.data.count)
    bytes.data.append(payload)
    bytes.data.append(Data(repeating: 0xAB, count: 300)) // the signature after the pack
    return bytes.data
}

@Suite struct PackTests {
    @Test func godot3PackBesideItsExecutable() throws {
        let game = try GameFolder([
            "game.pck": pack(format: 1, version: (3, 5, 3), files: ["res://project.binary", "res://main.gdc"]),
            "game.exe": Data("MZ".utf8),
        ])
        let layout = try #require(GodotLayout(game.probe))
        #expect(layout.pack == "game.pck")
        #expect(layout.executable == "game.exe")
        #expect(layout.version == "3.5.3")
        #expect(layout.header.files == ["project.binary", "main.gdc"])
        #expect(GodotPlugin.unsupported(layout) == nil)
    }

    @Test func godot4PackInTheExecutablesSection() throws {
        let game = try GameFolder([
            "Game V1.exe": executable(with: pack(format: 3, version: (4, 5, 1), files: ["project.binary"], flags: 2)),
            "Game V1.console.exe": Data("MZ".utf8),
        ])
        let layout = try #require(GodotLayout(game.probe))
        #expect(layout.pack == "Game V1.exe")
        #expect(layout.executable == "Game V1.exe")
        #expect(layout.version == "4.5.1")
        #expect(layout.header.files == ["project.binary"])
    }

    @Test func packAppendedToTheExecutable() throws {
        var file = Data("MZ".utf8) + Data(count: 100)
        let payload = pack(format: 2, version: (4, 3, 0), files: ["res://project.binary"])
        file.append(payload)
        var trailer = Bytes()
        trailer.u64(UInt64(payload.count))
        trailer.u32(GodotPack.magic)
        file.append(trailer.data)
        let game = try GameFolder(["game.exe": file])
        let layout = try #require(GodotLayout(game.probe))
        #expect(layout.version == "4.3.0")
        #expect(layout.header.offset == 102)
    }

    @Test func nestedGameWithALoader() throws {
        let game = try GameFolder([
            "Release/Game/Game.pck": pack(format: 1, version: (3, 3, 5), files: ["res://project.binary"]),
            "Release/Game/Game_Loader.exe": Data("MZ".utf8),
            "Release/Game/Game.exe": Data("MZ".utf8),
        ])
        let layout = try #require(GodotLayout(game.probe))
        #expect(layout.pack == "Release/Game/Game.pck")
        #expect(layout.executable == "Release/Game/Game.exe")
    }

    @Test func gdExtensionGamesAreUnsupported() throws {
        let game = try GameFolder([
            "game.exe": executable(with: pack(format: 3, version: (4, 5, 1), files: ["addons/godotsteam/godotsteam.gdextension", "project.binary"], flags: 2)),
        ])
        let layout = try #require(GodotLayout(game.probe))
        #expect(GodotPlugin.unsupported(layout)?.contains("godotsteam.gdextension") == true)
    }

    @Test func encryptedPacksAreUnsupported() throws {
        let game = try GameFolder(["game.pck": pack(format: 2, version: (4, 2, 2), files: [], flags: 1), "game.exe": Data()])
        let layout = try #require(GodotLayout(game.probe))
        #expect(layout.header.encryptedDirectory)
        #expect(GodotPlugin.unsupported(layout) != nil)
    }

    @Test func otherGamesAreNotGodot() throws {
        let game = try GameFolder(["Game.exe": Data("MZ".utf8) + Data(count: 400), "data.xp3": Data("XP3".utf8)])
        #expect(GodotLayout(game.probe) == nil)
    }
}

@Suite struct InputTests {
    @Test func controllerButtonsReachTheGameAsTheXboxController() {
        var router = GodotInputRouter()
        #expect(router.translate(.gamepad(.a, pressed: true)) == [.joyButton(0, down: true)])
        #expect(router.translate(.gamepad(.leftStickLeft, pressed: true)) == [.joyAxis(0, -1)])
        #expect(router.translate(.gamepad(.leftStickLeft, pressed: false)) == [.joyAxis(0, 0)])
        #expect(router.translate(.gamepad(.r2, pressed: true)) == [.joyAxis(5, 1)])
    }

    @Test func aKeyHeldTwiceGoesUpWithTheLastRelease() {
        var router = GodotInputRouter()
        #expect(router.translate(.button(.confirm, pressed: true)) == [.key(40, down: true)])
        #expect(router.translate(.key(KeyboardKey(key: "Enter", code: "Enter", keyCode: 13), pressed: true)).isEmpty)
        #expect(router.translate(.button(.confirm, pressed: false)).isEmpty)
        #expect(router.translate(.key(KeyboardKey(key: "Enter", code: "Enter", keyCode: 13), pressed: false)) == [.key(40, down: false)])
    }

    @Test func releaseAllLetsGoOfEverything() {
        var router = GodotInputRouter()
        _ = router.translate(.gamepad(.b, pressed: true))
        _ = router.hostKey(4, pressed: true)
        let released = router.releaseAll()
        #expect(Set(released.map { "\($0)" }) == Set(["\(GodotInput.joyButton(1, down: false))", "\(GodotInput.key(4, down: false))"]))
        #expect(router.translate(.gamepad(.b, pressed: false)).isEmpty)
    }

    @Test func webKeyCodesMapToHIDUsages() {
        #expect(HIDKey.usage(code: "KeyA") == 4)
        #expect(HIDKey.usage(code: "KeyZ") == 29)
        #expect(HIDKey.usage(code: "Digit1") == 30)
        #expect(HIDKey.usage(code: "Digit0") == 39)
        #expect(HIDKey.usage(code: "F1") == 58)
        #expect(HIDKey.usage(code: "ArrowUp") == 82)
        #expect(HIDKey.usage(code: "Numpad0") == 98)
        #expect(HIDKey.usage(code: "Numpad1") == 89)
    }
}
