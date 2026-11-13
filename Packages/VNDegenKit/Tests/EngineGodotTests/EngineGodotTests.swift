import CommonCrypto
import CryptoKit
import Foundation
import TestSupport
import Testing
import VNCore
@testable import EngineGodot

/// Builds a game folder from relative paths and file contents, and deletes it afterwards.
private final class GameFolder {
    let root: URL

    init(_ files: [String: Data]) throws {
        root = TestFolders.root.appendingPathComponent("godot-\(UUID().uuidString)", isDirectory: true)
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

/// A pack as Godot exports it. Format 1 is Godot 3's, 2 Godot 4.0 to 4.4's, 3 Godot 4.5's. Files are empty unless
/// `contents` has them. Offsets count from the pack's start, as Godot writes them for an executable's section.
private func pack(format: UInt32, version: (UInt32, UInt32, UInt32), files: [String], flags: UInt32 = 0,
                  contents: [String: Data] = [:], encrypted: Set<String> = [], directoryKey: Data? = nil) -> Data {
    let headerSize = 20 + (format >= 2 ? 12 : 0) + (format >= 3 ? 8 : 0) + 64
    var directorySize = 4
    for file in files {
        directorySize += 4 + (Data(file.utf8).count + 3) / 4 * 4 + 8 + 8 + 16 + (format >= 2 ? 4 : 0)
    }
    var blobs = Data()
    var directory = Bytes()
    directory.u32(UInt32(files.count))
    for file in files {
        var name = Data(file.utf8)
        name.append(Data(count: (4 - name.count % 4) % 4))
        directory.u32(UInt32(name.count))
        directory.data.append(name)
        let data = contents[file.replacingOccurrences(of: "res://", with: "")] ?? Data()
        directory.u64(UInt64(headerSize + directorySize + blobs.count))
        directory.u64(UInt64(data.count))
        directory.zeros(16)
        if format >= 2 { directory.u32(encrypted.contains(file) ? 1 : 0) }
        blobs.append(data)
    }
    var bytes = Bytes()
    bytes.u32(GodotPack.magic)
    bytes.u32(format)
    bytes.u32(version.0)
    bytes.u32(version.1)
    bytes.u32(version.2)
    if format >= 2 {
        // File offsets count from the pack's start, which the relative flag says for format 2.
        bytes.u32(flags | (format == 2 ? 2 : 0) | (directoryKey == nil ? 0 : 1))
        bytes.u64(0)
    }
    if format >= 3 {
        bytes.u64(UInt64(bytes.data.count + 8 + 64))
    }
    bytes.zeros(64)
    // Godot encrypts the entries after the count, and leaves the count as it is.
    bytes.data.append(directoryKey.map { directory.data.prefix(4) + encrypt(directory.data.dropFirst(4), key: $0) } ?? directory.data)
    bytes.data.append(blobs)
    return bytes.data
}

/// `data` encrypted as Godot writes an encrypted file inside a pack: its MD5, its length, and the IV, then the data in
/// AES-256 CFB, padded to whole blocks.
private func encrypt(_ data: Data, key: Data) -> Data {
    let iv = Data((0..<16).map { UInt8($0 * 7) })
    var plain = data
    plain.append(Data(count: (16 - data.count % 16) % 16))
    var cipher = Data(count: plain.count)
    var cryptor: CCCryptorRef?
    var moved = 0
    _ = key.withUnsafeBytes { key in
        iv.withUnsafeBytes { iv in
            CCCryptorCreateWithMode(CCOperation(kCCEncrypt), CCMode(kCCModeCFB), CCAlgorithm(kCCAlgorithmAES), CCPadding(ccNoPadding),
                                    iv.baseAddress, key.baseAddress, key.count, nil, 0, 0, CCModeOptions(0), &cryptor)
        }
    }
    _ = cipher.withUnsafeMutableBytes { cipher in
        plain.withUnsafeBytes { plain in CCCryptorUpdate(cryptor, plain.baseAddress, plain.count, cipher.baseAddress, cipher.count, &moved) }
    }
    CCCryptorRelease(cryptor)
    var bytes = Bytes()
    bytes.data.append(Data(Insecure.MD5.hash(data: data)))
    bytes.u64(UInt64(data.count))
    bytes.data.append(iv)
    bytes.data.append(cipher)
    return bytes.data
}

/// A `project.binary` with string and boolean settings, as Godot encodes them.
private func projectBinary(_ settings: [(String, Any)]) -> Data {
    var bytes = Bytes()
    bytes.data.append(Data("ECFG".utf8))
    bytes.u32(UInt32(settings.count))
    for (key, value) in settings {
        bytes.u32(UInt32(Data(key.utf8).count))
        bytes.data.append(Data(key.utf8))
        var variant = Bytes()
        if let text = value as? String {
            variant.u32(4)
            variant.u32(UInt32(Data(text.utf8).count))
            variant.data.append(Data(text.utf8))
            variant.zeros((4 - Data(text.utf8).count % 4) % 4)
        } else if let flag = value as? Bool {
            variant.u32(1)
            variant.u32(flag ? 1 : 0)
        }
        bytes.u32(UInt32(variant.data.count))
        bytes.data.append(variant.data)
    }
    return bytes.data
}

/// A 64-bit Windows executable with `payload` in a section named "pck", followed by a signature. It imports `imports`,
/// and its `.data` section holds `data`.
private func executable(with payload: Data, data: Data = Data(count: 64), imports: [String] = []) -> Data {
    let sectionTable = 0x88 + 24 + 0xF0
    var idata = Bytes()
    let names = imports.reduce(into: (offsets: [Int](), bytes: Data())) { result, name in
        result.offsets.append((imports.count + 1) * 20 + result.bytes.count)
        result.bytes.append(Data(name.utf8) + Data([0]))
    }
    for offset in names.offsets {
        idata.zeros(12)
        idata.u32(UInt32(0x2000 + offset)) // the name's address
        idata.zeros(4)
    }
    idata.zeros(20)
    idata.data.append(names.bytes)
    // Name, address, size, and data for each section. Raw data starts at 0x400, each part 0x200-aligned.
    let sections: [(String, UInt32, Data)] = [(".text", 0x1000, Data(count: 16)), (".data", 0x3000, data), (".idata", 0x2000, idata.data), ("pck", 0x9000, payload)]
    var raw = 0x400
    var placed: [(String, UInt32, Data, Int)] = []
    for (name, address, contents) in sections {
        placed.append((name, address, contents, raw))
        raw += (contents.count + 0x1FF) / 0x200 * 0x200
    }

    var bytes = Bytes()
    bytes.data.append(Data("MZ".utf8))
    bytes.zeros(0x3C - 2)
    bytes.u32(0x88) // PE header offset
    bytes.zeros(0x88 - bytes.data.count)
    bytes.u32(0x0000_4550) // "PE\0\0"
    bytes.data.append(contentsOf: [0x64, 0x86]) // machine
    bytes.data.append(contentsOf: [UInt8(placed.count), 0])
    bytes.zeros(12)
    bytes.data.append(contentsOf: [0xF0, 0]) // optional header size
    bytes.zeros(2)
    bytes.data.append(contentsOf: [0x0B, 0x02]) // PE32+
    bytes.zeros(110)
    bytes.zeros(8) // export table
    bytes.u32(0x2000) // import table
    bytes.u32(UInt32(idata.data.count))
    bytes.zeros(sectionTable - bytes.data.count)
    for (name, address, contents, offset) in placed {
        var field = Data(name.utf8)
        field.append(Data(count: 8 - field.count))
        bytes.data.append(field)
        bytes.u32(UInt32(name == "pck" ? 8 : contents.count)) // Godot's pck section claims a tiny size once loaded
        bytes.u32(address)
        bytes.u32(UInt32((contents.count + 0x1FF) / 0x200 * 0x200))
        bytes.u32(UInt32(offset))
        bytes.zeros(16)
    }
    for (_, _, contents, offset) in placed {
        bytes.zeros(offset - bytes.data.count)
        bytes.data.append(contents)
    }
    bytes.zeros(raw - bytes.data.count)
    bytes.data.append(Data(repeating: 0xAB, count: 300)) // the signature after the pack
    return bytes.data
}

/// Why VNDegen can't run the game, or nil when it can.
private func refusal(_ layout: GodotLayout, _ game: GameFolder) -> String? {
    guard case let .failure(.unsupported(reason)) = GodotPlugin.requirements(layout, folder: game.root) else { return nil }
    return reason
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
        #expect(refusal(layout, game) == nil)
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
            "game.exe": executable(with: pack(format: 3, version: (4, 5, 1), files: ["addons/godotsteam/godotsteam.gdextension", "addons/fmod/fmod.gdextension", "project.binary"], flags: 2)),
        ])
        let layout = try #require(GodotLayout(game.probe))
        #expect(refusal(layout, game)?.contains("(fmod.gdextension)") == true)
    }

    @Test func steamAndEpicExtensionsGetStandIns() throws {
        let game = try GameFolder([
            "game.exe": executable(with: pack(format: 3, version: (4, 5, 1), files: ["addons/godotsteam/godotsteam.gdextension", "addons/epic-online-services-godot/eosg.gdextension", "project.binary"], flags: 2)),
        ])
        let layout = try #require(GodotLayout(game.probe))
        let requirements = try GodotPlugin.requirements(layout, folder: game.root).get()
        #expect(requirements.steam)
        #expect(requirements.epicOnlineServices)
    }

    @Test func steamBuiltIntoTheGamesGodot() throws {
        let game = try GameFolder([
            "Game.pck": pack(format: 1, version: (3, 3, 5), files: ["res://project.binary"]),
            "Game.exe": executable(with: Data(), imports: ["KERNEL32.dll", "steam_api64.dll"]),
        ])
        let layout = try #require(GodotLayout(game.probe))
        #expect(try GodotPlugin.requirements(layout, folder: game.root).get().steam)
        let plain = try GameFolder(["Game.pck": pack(format: 1, version: (3, 3, 5), files: ["res://project.binary"]),
                                    "Game.exe": executable(with: Data(), imports: ["KERNEL32.dll"])])
        #expect(try !GodotPlugin.requirements(#require(GodotLayout(plain.probe)), folder: plain.root).get().steam)
    }

    @Test func theKeyOfAnEncryptedPackComesFromTheExecutable() throws {
        let key = Data((0..<32).map { UInt8(0x40 + $0) })
        let script = Data("extends Node2D\n".utf8) // shorter than an AES block, so padded
        // The key sits among other data, at a multiple of four as compilers place it.
        let data = Data(repeating: 0x11, count: 4092) + key + Data(repeating: 0x22, count: 100)
        let settings = projectBinary([("application/config/name", "Sealed")])
        let files = ["res://project.binary", "res://main.gd", "res://big.gd"]
        let game = try GameFolder([
            "game.exe": executable(with: pack(format: 3, version: (4, 5, 1), files: files, flags: 2,
                                              contents: ["project.binary": encrypt(settings, key: key), "main.gd": encrypt(script, key: key),
                                                         "big.gd": encrypt(script + script, key: key)],
                                              encrypted: Set(files)), data: data),
        ])
        let layout = try #require(GodotLayout(game.probe))
        #expect(layout.header.encryptedFiles == 3)
        #expect(try GodotPlugin.requirements(layout, folder: game.root).get().encryptionKey == key)
        // Saves go where the decrypted settings say.
        #expect(layout.header.userDirectory == "Godot/app_userdata/Sealed")
    }

    @Test func textThatIsntASCIIFallsBackToAnotherFile() throws {
        let key = Data((0..<32).map { UInt8(truncatingIfNeeded: 0x31 * $0 + 9) })
        let files = ["res://project.binary", "res://credits.txt", "res://icon.png"]
        let game = try GameFolder([
            "game.exe": executable(with: pack(format: 3, version: (4, 5, 1), files: files, flags: 2,
                                              contents: ["credits.txt": encrypt(Data("é\n".utf8), key: key),
                                                         "icon.png": encrypt(Data(repeating: 0x89, count: 40), key: key)],
                                              encrypted: ["res://credits.txt", "res://icon.png"]), data: Data(repeating: 0x55, count: 128) + key),
        ])
        let layout = try #require(GodotLayout(game.probe))
        #expect(layout.header.encryptionKey == key)
    }

    @Test func aFileWhoseStartCantBeToldStillGivesTheKey() throws {
        let key = Data((0..<32).map { UInt8(truncatingIfNeeded: 0x10 * $0 + 5) })
        let game = try GameFolder([
            "game.exe": executable(with: pack(format: 3, version: (4, 5, 1), files: ["res://project.binary", "res://icon.png"], flags: 2,
                                              contents: ["icon.png": encrypt(Data((0..<300).map { UInt8($0 % 251) }), key: key)],
                                              encrypted: ["res://icon.png"]), data: Data(repeating: 0x44, count: 256) + key),
        ])
        let layout = try #require(GodotLayout(game.probe))
        #expect(layout.header.encryptionKey == key)
    }

    @Test func theKeyOfAnEncryptedFileList() throws {
        let key = Data((0..<32).map { UInt8(0x90 &+ $0 &* 3) })
        let game = try GameFolder([
            "game.exe": executable(with: pack(format: 3, version: (4, 5, 1), files: ["a.txt", "addons/godotsteam/godotsteam.gdextension"],
                                              flags: 2, directoryKey: key),
                                   data: Data(repeating: 0x33, count: 64) + key),
        ])
        let layout = try #require(GodotLayout(game.probe))
        #expect(layout.header.encryptedDirectory)
        // The decrypted list shows what the game needs.
        // The first path is short, so zeros pad it in the list.
        #expect(layout.header.files == ["a.txt", "addons/godotsteam/godotsteam.gdextension"])
        let requirements = try GodotPlugin.requirements(layout, folder: game.root).get()
        #expect(requirements.encryptionKey == key)
        #expect(requirements.steam)
    }

    @Test func encryptedPacksWithoutTheirKeyAreUnsupported() throws {
        let game = try GameFolder(["game.pck": pack(format: 2, version: (4, 2, 2), files: ["res://project.binary"], directoryKey: Data(count: 32)), "game.exe": Data()])
        let layout = try #require(GodotLayout(game.probe))
        #expect(layout.header.encryptedDirectory)
        #expect(refusal(layout, game)?.contains("encrypted") == true)
    }

    @Test func projectSettingsNameTheUserFolder() throws {
        let settings = projectBinary([("application/config/features", "4.3"), ("application/config/name", "My: Game")])
        let game = try GameFolder(["game.pck": pack(format: 2, version: (4, 3, 0), files: ["res://project.binary"], contents: ["project.binary": settings]),
                                   "game.exe": Data()])
        let layout = try #require(GodotLayout(game.probe))
        #expect(layout.header.settings["application/config/name"] == .string("My: Game"))
        #expect(layout.header.userDirectory == "Godot/app_userdata/My- Game")
    }

    @Test func aLongFileListIsReadWhole() throws {
        let files = (0..<10_000).map { String(format: "res://assets/sprites/sprite_%05d.png.import", $0) } + ["res://project.binary"]
        let settings = projectBinary([("application/config/name", "Long")])
        let game = try GameFolder(["game.pck": pack(format: 2, version: (4, 3, 0), files: files, contents: ["project.binary": settings]),
                                   "game.exe": Data()])
        let layout = try #require(GodotLayout(game.probe))
        #expect(layout.header.files == files.map { String($0.dropFirst(6)) })
        #expect(layout.header.settings["application/config/name"] == .string("Long"))
    }

    @Test func aCustomUserFolder() throws {
        let settings = projectBinary([("application/config/name", "Game"), ("application/config/use_custom_user_dir", true),
                                      ("application/config/custom_user_dir_name", "Studio/Game")])
        let game = try GameFolder(["game.exe": executable(with: pack(format: 3, version: (4, 5, 1), files: ["project.binary"], flags: 2,
                                                                      contents: ["project.binary": settings]))])
        let layout = try #require(GodotLayout(game.probe))
        #expect(layout.header.userDirectory == "Studio/Game")
    }

    @Test func userFoldersAreNamedAsTheRuntimeNamesThem() {
        func folder(_ settings: [String: GodotPack.Setting], major: Int) -> String {
            var pack = GodotPack(major: major, minor: 0, patch: 0, format: major == 3 ? 1 : 3, offset: 0, encryptedDirectory: false, files: [])
            pack.settings = settings
            return pack.userDirectory
        }
        let name = "application/config/name", custom = "application/config/custom_user_dir_name"
        let useCustom = "application/config/use_custom_user_dir"
        // Godot 4 drops trailing periods and renames "." and ".."; Godot 3 keeps them.
        #expect(folder([name: .string(" Example. ")], major: 4) == "Godot/app_userdata/Example")
        #expect(folder([name: .string("Example.")], major: 3) == "Godot/app_userdata/Example.")
        #expect(folder([name: .string("..")], major: 4) == "Godot/app_userdata/twodots")
        #expect(folder([name: .string("a/b\\c")], major: 4) == "Godot/app_userdata/a-b-c")
        #expect(folder([name: .string("Game"), useCustom: .bool(true), custom: .string("Studio\\\\Game.")], major: 4) == "Studio/Game")
        #expect(folder([name: .string("Game"), useCustom: .bool(true), custom: .string("../Game")], major: 3) == "-/Game")
        // A project without a name gets the unnamed folder, even with a custom folder.
        #expect(folder([useCustom: .bool(true), custom: .string("Studio")], major: 4) == "Godot/app_userdata/[unnamed project]")
    }

    @Test func malformedPacksAreNotRead() throws {
        // A trailer whose size runs past the start of the file.
        var trailer = Bytes()
        trailer.u32(0)
        trailer.u64(.max)
        trailer.u32(GodotPack.magic)
        // A format 3 header whose directory offset and file base are out of range.
        var header = Bytes()
        for value: UInt32 in [GodotPack.magic, 3, 4, 5, 1, 2] { header.u32(value) }
        header.u64(.max)
        header.u64(.max)
        header.zeros(64)
        let game = try GameFolder(["trailer.exe": Data("MZ".utf8) + trailer.data, "header.pck": header.data, "header.exe": Data()])
        #expect(GodotPack(contentsOf: game.root.appendingPathComponent("trailer.exe")) == nil)
        let pack = try #require(GodotPack(contentsOf: game.root.appendingPathComponent("header.pck")))
        #expect(pack.files.isEmpty)
    }

    @Test func csharpGamesAreUnsupported() throws {
        let game = try GameFolder([
            "Game.pck": pack(format: 2, version: (4, 2, 1), files: ["res://project.binary"]),
            "Game.exe": Data("MZ".utf8),
            "data_Game_windows_x86_64/GodotSharp.dll": Data(),
        ])
        let layout = try #require(GodotLayout(game.probe))
        #expect(layout.usesDotNet)
        #expect(refusal(layout, game)?.contains("C#") == true)
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

    @Test func oppositeDirectionsCancelAndReleasingOneLeavesTheOther() {
        var router = GodotInputRouter()
        #expect(router.translate(.gamepad(.leftStickLeft, pressed: true)) == [.joyAxis(0, -1)])
        #expect(router.translate(.gamepad(.leftStickRight, pressed: true)) == [.joyAxis(0, 0)])
        #expect(router.translate(.gamepad(.leftStickRight, pressed: false)) == [.joyAxis(0, -1)])
        #expect(router.translate(.gamepad(.leftStickLeft, pressed: false)) == [.joyAxis(0, 0)])
    }

    @Test func scrollingDownTurnsTheWheelDown() {
        var router = GodotInputRouter()
        #expect(router.translate(.scroll(dy: GodotInputRouter.pointsPerNotch)) == [.wheel(-1)])
    }

    @Test func hardwareKeysCarryTheCharacterTheyType() {
        var router = GodotInputRouter()
        #expect(router.hostKey(4, pressed: true, text: "a") == [.key(4, down: true, character: 0x61)])
        #expect(router.hostKey(4, pressed: false) == [.key(4, down: false)])
        // Return, an arrow's private-use character from AppKit, and nothing typed.
        #expect(router.hostKey(40, pressed: true, text: "\r") == [.key(40, down: true)])
        #expect(router.hostKey(82, pressed: true, text: "\u{F700}") == [.key(82, down: true)])
        #expect(router.hostKey(225, pressed: true, text: nil) == [.key(225, down: true)])
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

@Suite struct SaveTests {
    @Test func backupsFromEveryPlatformMapIntoTheUserFolder() {
        let user = "Godot/app_userdata/Game"
        let windows = "C:/Users/Player/AppData/Roaming/Godot/app_userdata/Game/save1.json"
        let proton = "/home/deck/.local/share/Steam/steamapps/compatdata/1/pfx/drive_c/users/steamuser/AppData/Roaming/Godot/app_userdata/Game/saves/a.sav"
        let linux = "/home/deck/.local/share/godot/app_userdata/Game/save1.json"
        let mac = "/Users/player/Library/Application Support/Godot/app_userdata/Game/save1.json"
        let vndegen = "/private/var/mobile/Containers/Data/Application/X/Library/Application Support/VNDegen/GameData/Game/AppData/Roaming/Godot/app_userdata/Game/save1.json"
        #expect(GodotPlugin.userDataRelativePath(windows, userDirectory: user) == "save1.json")
        #expect(GodotPlugin.userDataRelativePath(proton, userDirectory: user) == "saves/a.sav")
        #expect(GodotPlugin.userDataRelativePath(linux, userDirectory: user) == "save1.json")
        #expect(GodotPlugin.userDataRelativePath(mac, userDirectory: user) == "save1.json")
        #expect(GodotPlugin.userDataRelativePath(vndegen, userDirectory: user) == "save1.json")
    }

    @Test func otherFoldersAndEscapesDontMap() {
        let user = "Godot/app_userdata/Game"
        #expect(GodotPlugin.userDataRelativePath("C:/Users/P/AppData/Roaming/Godot/app_userdata/Other/save1.json", userDirectory: user) == nil)
        #expect(GodotPlugin.userDataRelativePath("C:/Users/P/AppData/Roaming/Godot/app_userdata/Game/../x", userDirectory: user) == nil)
        #expect(GodotPlugin.userDataRelativePath("C:/Users/P/AppData/Roaming/Godot/app_userdata/Game", userDirectory: user) == nil)
    }

    @Test func savesLiveInTheAppDataStandIn() throws {
        let settings = projectBinary([("application/config/name", "Game")])
        let game = try GameFolder(["game.pck": pack(format: 1, version: (3, 5, 3), files: ["res://project.binary"], contents: ["project.binary": settings]),
                                   "game.exe": Data()])
        let data = URL(fileURLWithPath: "/tmp/data")
        let location = try #require(GodotPlugin().saveLocations(folder: game.root, dataDirectory: data).first)
        #expect(location.directory.path == "/tmp/data/AppData/Roaming/Godot/app_userdata/Game")
        #expect(location.relativePath("/tmp/data/AppData/Roaming/Godot/app_userdata/Game/save1.json") == "save1.json")
    }
}

@MainActor @Test func cheatsOfferGameSpeedOnly() async throws {
    var speeds: [Int] = []
    let cheats = GodotCheats { speeds.append($0) }
    #expect(cheats.features == .speed)
    #expect(try await cheats.state().inGame)
    var settings = CheatSettings()
    settings.speed = 4
    cheats.apply(settings)
    #expect(speeds == [4])
}

private enum Event {
    case key(Int), joyButton(Int), joyAxis(Int, Float), mouse(Int)
}

private func variantString(_ text: String, into bytes: inout Bytes) {
    bytes.u32(UInt32(Data(text.utf8).count))
    bytes.data.append(Data(text.utf8))
    bytes.zeros((4 - Data(text.utf8).count % 4) % 4)
}

private func inputBinary(_ actions: [(String, [Event])], major: Int) -> Data {
    let (object, dictionary, array): (UInt32, UInt32, UInt32) = major >= 4 ? (24, 27, 28) : (17, 18, 19)
    var bytes = Bytes()
    bytes.data.append(Data("ECFG".utf8))
    bytes.u32(UInt32(actions.count))
    for (name, events) in actions {
        let key = "input/\(name)"
        bytes.u32(UInt32(Data(key.utf8).count))
        bytes.data.append(Data(key.utf8))
        var value = Bytes()
        value.u32(dictionary)
        value.u32(2)
        value.u32(4)
        variantString("deadzone", into: &value)
        value.u32(3)
        value.data.append(contentsOf: withUnsafeBytes(of: Float(0.5).bitPattern.littleEndian, Array.init))
        value.u32(4)
        variantString("events", into: &value)
        value.u32(array)
        value.u32(UInt32(events.count))
        for event in events {
            value.u32(object)
            let properties: [(String, UInt32, UInt32)]
            switch event {
            case let .key(code):
                variantString("InputEventKey", into: &value)
                properties = [(major >= 4 ? "keycode" : "scancode", 2, UInt32(code)), (major >= 4 ? "physical_keycode" : "physical_scancode", 2, 0),
                              (major >= 4 ? "shift_pressed" : "shift", 1, 0)]
            case let .joyButton(index):
                variantString("InputEventJoypadButton", into: &value)
                properties = [("button_index", 2, UInt32(index))]
            case let .joyAxis(axis, sign):
                variantString("InputEventJoypadMotion", into: &value)
                properties = [("axis", 2, UInt32(axis)), ("axis_value", 3, sign.bitPattern)]
            case let .mouse(index):
                variantString("InputEventMouseButton", into: &value)
                properties = [("button_index", 2, UInt32(index)), ("position", 5, 0)]
            }
            value.u32(UInt32(properties.count))
            for (property, type, raw) in properties {
                variantString(property, into: &value)
                value.u32(type)
                value.u32(raw)
                if type == 5 { value.u32(0) }
            }
        }
        bytes.u32(UInt32(value.data.count))
        bytes.data.append(value.data)
    }
    return bytes.data
}

private let special4 = 1 << 22
private let hadal: [(String, [Event])] = [
    ("MOVEUP", [.key(special4 + 0x10), .key(87)]),
    ("MOVEDOWN", [.key(special4 + 0x12), .key(83)]),
    ("MOVELEFT", [.key(special4 + 0x0F), .key(65)]),
    ("MOVERIGHT", [.key(special4 + 0x11), .key(68)]),
    ("LEFTMOUSE", [.mouse(1)]),
    ("RIGHTMOUSE", [.mouse(2)]),
    ("SPACEBAR", [.key(32)]),
    ("INVENTORY", [.key(73)]),
    ("debug_spawn", [.key(80)]),
]

private func key(_ code: Int) -> ControlAction { .key(KeyboardKey(windowsKeyCode: code)!) }

@Suite struct SuggestedControlsTests {
    @Test func aGameWithNoJoypadBindingsGetsMovementOnTheDpadAndStick() throws {
        let map = GodotControls.inputMap(inputBinary(hadal, major: 4), major: 4)
        #expect(map.map(\.name) == hadal.map(\.0))
        let suggestion = try #require(GodotControls.suggestion(map, major: 4))
        #expect(suggestion.buttons[.up] == key(38) && suggestion.buttons[.leftStickUp] == key(38))
        #expect(suggestion.buttons[.left] == key(37) && suggestion.buttons[.leftStickRight] == key(39))
        #expect(suggestion.buttons[.a] == key(32))
        #expect(suggestion.buttons[.r2] == .click(.primary) && suggestion.buttons[.l2] == .click(.secondary))
        #expect(suggestion.buttons[.rightStickUp] == .mouseMove(.up) && suggestion.buttons[.rightStickLeft] == .mouseMove(.left))
        #expect(suggestion.buttons[.x] == key(73))
        #expect(suggestion.labels[.x] == "Inventory" && suggestion.labels[.up] == "Moveup")
        #expect(!suggestion.buttons.values.contains(key(80)))
        #expect(suggestion.buttons[.b] == nil)
    }

    @Test func aButtonAnActionReadsWithoutTheKeyIsLeftAlone() throws {
        let map = GodotControls.inputMap(inputBinary([
            ("ui_up", [.key(special4 + 0x10), .joyButton(11)]),
            ("climb", [.joyButton(11)]),
            ("MOVEUP", [.key(special4 + 0x10)]),
        ], major: 4), major: 4)
        let suggestion = try #require(GodotControls.suggestion(map, major: 4))
        #expect(suggestion.buttons[.up] == nil)
        #expect(suggestion.buttons[.leftStickUp] == key(38))
    }

    @Test func keysOnlyGoToButtonsTheGameLeavesFree() throws {
        let map = GodotControls.inputMap(inputBinary([
            ("jump", [.key(32), .joyButton(0)]),
            ("dash", [.key(81)]),
            ("run", [.key(special4 + 0x15)]),
            ("pause", [.key(special4 + 0x01), .joyButton(6)]),
            ("look", [.mouse(1)]),
            ("aim", [.joyAxis(2, -1), .joyAxis(2, 1), .joyAxis(3, -1), .joyAxis(3, 1)]),
        ], major: 4), major: 4)
        let suggestion = try #require(GodotControls.suggestion(map, major: 4))
        #expect(suggestion.buttons == [.x: key(81), .r: key(16)])
        #expect(suggestion.labels == [.x: "Dash", .r: "Run"])
    }

    @Test func godot3NumbersItsButtonsAndKeysItsOwnWay() throws {
        let special3 = 1 << 24
        let map = GodotControls.inputMap(inputBinary([
            ("ui_accept", [.key(90), .joyButton(0), .joyButton(1), .joyButton(2), .joyButton(3)]),
            ("pages", [.joyButton(4), .joyButton(5), .joyButton(6), .joyButton(7)]),
            ("save", [.key(special3 + 0x1C + 4)]),
        ], major: 3), major: 3)
        let suggestion = try #require(GodotControls.suggestion(map, major: 3))
        #expect(suggestion.buttons == [.view: key(116)])
    }

    @Test func aGameBindingEveryButtonGetsNothing() {
        let map = GodotControls.inputMap(inputBinary([
            ("all", (0...14).map(Event.joyButton) + [.joyAxis(4, 1), .joyAxis(5, 1)]),
            ("menu", [.key(77)]),
        ], major: 4), major: 4)
        #expect(GodotControls.suggestion(map, major: 4) == nil)
    }

    @Test func scriptsThatReadTheJoypadOrCantBeReadStopASuggestion() {
        let text = Data("func _process(d):\n\tif Input.is_joy_button_pressed(0, JOY_BUTTON_A):\n\t\tpass\n".utf8)
        #expect(GodotControls.readsJoypad(text) == true)
        #expect(GodotControls.readsJoypad(Data("func _ready():\n\tpass\n".utf8)) == false)
        let godot3 = Data("GDSC".utf8) + Data([13, 0, 0, 0]) + Data(repeating: 0, count: 8) + Data("get_joy_axis".utf8.map { $0 ^ 0xB6 })
        #expect(GodotControls.readsJoypad(godot3) == true)
        let godot4 = Data("GDSC".utf8) + Data([101, 0, 0, 0, 0, 0, 0, 0]) + Data("InputEventJoypadButton".utf8.flatMap { [$0 ^ 0xB6, 0xB6, 0xB6, 0xB6] })
        #expect(GodotControls.readsJoypad(godot4) == true)
        let compressed = Data("GDSC".utf8) + Data([100, 0, 0, 0, 0x48, 0xC2, 0, 0, 0x28, 0xB5, 0x2F, 0xFD])
        #expect(GodotControls.readsJoypad(compressed) == nil)
        let clean = GodotScript(name: "main.gd", offset: 0, size: 4, encrypted: false)
        #expect(GodotControls.readsJoypad([], read: { _, _ in Data() }) == nil)
        #expect(GodotControls.readsJoypad([GodotScript(name: "main.gde", offset: 0, size: 4, encrypted: false)], read: { _, _ in Data("pass".utf8) }) == nil)
        #expect(GodotControls.readsJoypad([GodotScript(name: "main.gd", offset: 0, size: 4, encrypted: true)], read: { _, _ in Data("pass".utf8) }) == nil)
        #expect(GodotControls.readsJoypad([clean], read: { _, _ in Data("pass".utf8) }) == false)
    }

    private func suggestion(scripts: [String: Data]) throws -> ControlSuggestion? {
        let contents = ["project.binary": inputBinary(hadal, major: 4)].merging(scripts) { $1 }
        let files = ["res://project.binary"] + scripts.keys.sorted().map { "res://\($0)" }
        let game = try GameFolder(["game.pck": pack(format: 2, version: (4, 3, 0), files: files, contents: contents)])
        return GodotPlugin().suggestedControls(folder: game.root)
    }

    @Test func thePluginReadsThePacksInputMapAndScripts() throws {
        let clean = Data("extends Node\nfunc _input(e):\n\tif e.is_action_pressed(\"MOVEUP\"):\n\t\tpass\n".utf8)
        #expect(try suggestion(scripts: ["player.gd": clean])?.buttons[.up] == key(38))
        #expect(try suggestion(scripts: ["player.gd": clean, "pad.gd": Data("if event is InputEventJoypadMotion:\n".utf8)]) == nil)
        #expect(try suggestion(scripts: [:]) == nil)
        #expect(try suggestion(scripts: ["player.gdc": Data("GDSC".utf8) + Data([100, 0, 0, 0, 9, 0, 0, 0, 0x28, 0xB5, 0x2F, 0xFD])]) == nil)
    }
}
