import CNekoSDK
import Foundation
import Testing
import TestSupport

// Patches, as the library's games ship them: The Alchemist's sys_5.pak holds a second art set under the base names and a
// cfg that replaces the base one, Test Game 90's sys_2.pak new files the game names `sys_2\…`, and Test Game 92's sys_5
// folder archives of its own.
@Suite struct NekoSDKPatchTests {
    // A NEKOPACK4A archive: the index lists each name with its offset and size XORed with the sum of the name's signed bytes.
    // Entries' bytes don't matter here, only the sizes the index stores.
    private func pack(_ entries: [(String, Int)]) -> Data {
        var index = Data()
        var offset = 14 + entries.reduce(0) { $0 + 4 + $1.0.utf8.count + 1 + 8 }
        for (name, size) in entries {
            let bytes = Array(name.utf8) + [0]
            let key = bytes.reduce(UInt32(0)) { $0 &+ UInt32(bitPattern: Int32(Int8(bitPattern: $1))) }
            index.append(contentsOf: withUnsafeBytes(of: UInt32(bytes.count).littleEndian, Array.init))
            index.append(contentsOf: bytes)
            index.append(contentsOf: withUnsafeBytes(of: (UInt32(offset) ^ key).littleEndian, Array.init))
            index.append(contentsOf: withUnsafeBytes(of: (UInt32(size) ^ key).littleEndian, Array.init))
            offset += size
        }
        var data = Data("NEKOPACK4A".utf8)
        data.append(contentsOf: withUnsafeBytes(of: UInt32(index.count).littleEndian, Array.init))
        data.append(index)
        data.append(Data(count: entries.reduce(0) { $0 + $1.1 }))
        return data
    }

    // An archive entry holding `plain`: a stored zlib stream with its first 4 bytes XORed with the size's rolling key, and
    // the plain size at the end.
    private func packed(_ plain: Data) -> Data {
        var z = Data([0x78, 0x01, 0x01])
        z.append(contentsOf: withUnsafeBytes(of: UInt16(plain.count).littleEndian, Array.init))
        z.append(contentsOf: withUnsafeBytes(of: (~UInt16(plain.count)).littleEndian, Array.init))
        z.append(plain)
        var a: UInt32 = 1, b: UInt32 = 0
        for byte in plain { a = (a + UInt32(byte)) % 65521; b = (b + a) % 65521 }
        z.append(contentsOf: withUnsafeBytes(of: (b << 16 | a).bigEndian, Array.init))
        z.append(contentsOf: withUnsafeBytes(of: UInt32(plain.count).littleEndian, Array.init))
        var key = UInt32(z.count / 8 + 0x22)
        for i in 0..<4 { z[i] ^= UInt8(key & 0xff); key <<= 3 }
        return z
    }

    // A NEKOPACK4A archive of real entries.
    private func archive(_ entries: [(String, Data)]) -> Data {
        var index = Data(), body = Data()
        let header = 14 + entries.reduce(0) { $0 + 4 + $1.0.utf8.count + 1 + 8 }
        for (name, plain) in entries {
            let entry = packed(plain), bytes = Array(name.utf8) + [0]
            let key = bytes.reduce(UInt32(0)) { $0 &+ UInt32(bitPattern: Int32(Int8(bitPattern: $1))) }
            index.append(contentsOf: withUnsafeBytes(of: UInt32(bytes.count).littleEndian, Array.init))
            index.append(contentsOf: bytes)
            index.append(contentsOf: withUnsafeBytes(of: (UInt32(header + body.count) ^ key).littleEndian, Array.init))
            index.append(contentsOf: withUnsafeBytes(of: (UInt32(entry.count) ^ key).littleEndian, Array.init))
            body.append(entry)
        }
        var data = Data("NEKOPACK4A".utf8)
        data.append(contentsOf: withUnsafeBytes(of: UInt32(index.count).littleEndian, Array.init))
        return data + index + body
    }

    // A 1 by 1 24-bit BMP of one colour.
    private func bmp(_ r: UInt8, _ g: UInt8, _ b: UInt8) -> Data {
        var d = Data("BM".utf8)
        for v: UInt32 in [58, 0, 54] { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        for v: UInt32 in [40, 1, 1] { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        d.append(contentsOf: [1, 0, 24, 0])
        d.append(Data(count: 24))
        d.append(contentsOf: [b, g, r, 0])
        return d
    }

    private func game(_ files: [String: Data]) throws -> URL {
        let root = TestFolders.root.appendingPathComponent("nekosdk-patch-\(UUID().uuidString)")
        for (path, data) in files {
            let url = root.appendingPathComponent(path)
            try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try data.write(to: url)
        }
        return root
    }

    private func size(_ root: URL, _ path: String) -> Int64 { vnneko_debug_file_size(root.path, path) }

    @Test func aPatchArchiveReplacesAllButArt() throws {
        let root = try game([
            "cg.pak": pack([("a.bmp", 10)]),
            "sc.pak": pack([("b.bmp", 11)]),
            "cfg.pak": pack([("msg_button.ini", 12)]),
            "sys_5.pak": pack([("cg\\a.bmp", 20), ("sc\\b.bmp", 21), ("cfg\\msg_button.ini", 22), ("voice\\new.ogg", 23), ("loose.bmp", 24)]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        // The second art set stays out of the way until asked for by the patch's name.
        #expect(size(root, "cg\\a.bmp") == 10)
        #expect(size(root, "SC\\B.BMP") == 11)
        #expect(size(root, "sys_5\\cg\\a.bmp") == 20)
        #expect(size(root, "sys_5\\sc\\b.bmp") == 21)
        // Everything else takes the base file's place, and answers to the patch's name too.
        #expect(size(root, "cfg\\msg_button.ini") == 22)
        #expect(size(root, "sys_5\\cfg\\msg_button.ini") == 22)
        #expect(size(root, "voice\\new.ogg") == 23)
        #expect(size(root, "sys_5\\voice\\new.ogg") == 23)
        // A file outside any folder stands in for sys.pak's.
        #expect(size(root, "sys\\loose.bmp") == 24)
        #expect(size(root, "sys_5\\loose.bmp") == 24)
    }

    @Test func aPatchFolderOpensItsOwnArchives() throws {
        let root = try game([
            "obj.pak": pack([("c.bmp", 30)]),
            "scr.pak": pack([("x.txt", 31)]),
            "sys_5/obj.pak": pack([("c.bmp", 40)]),
            "sys_5/cfg.pak": pack([("anime_view.ini", 41)]),
            "sys_5/scr/x.txt": Data(count: 42),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(size(root, "obj\\c.bmp") == 30)
        #expect(size(root, "sys_5\\obj\\c.bmp") == 40)
        #expect(size(root, "cfg\\anime_view.ini") == 41)
        #expect(size(root, "sys_5\\cfg\\anime_view.ini") == 41)
        #expect(size(root, "scr\\x.txt") == 42)
        #expect(size(root, "sys_5\\scr\\x.txt") == 42)
        #expect(size(root, "sys_5\\obj.pak") == -1)
    }

    @Test func theArtSetSwitchesBothWays() throws {
        let root = try game([
            "cg.pak": archive([("a.bmp", bmp(255, 0, 0))]),
            "sys_5.pak": archive([("cg\\a.bmp", bmp(0, 0, 255))]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        var pixels: [UInt32] = [0, 0, 0]
        #expect(vnneko_debug_art_pixels(root.path, "cg\\a.bmp", "sys_5", &pixels))
        #expect(pixels == [0xff00_00ff, 0x0000_ffff, 0xff00_00ff])
        // A patch without that picture leaves the base one.
        #expect(vnneko_debug_art_pixels(root.path, "cg\\a.bmp", "sys_9", &pixels))
        #expect(pixels == [0xff00_00ff, 0xff00_00ff, 0xff00_00ff])
    }

    @Test func artNamedWithForwardSlashesStaysInItsPatch() throws {
        let root = try game([
            "cg.pak": pack([("a.bmp", 10)]),
            "sys_5.pak": pack([("cg/a.bmp", 20)]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(size(root, "cg\\a.bmp") == 10)
        #expect(size(root, "sys_5\\cg\\a.bmp") == 20)
    }

    @Test func aPatchFoldersArchivesKeepTheirFolderName() throws {
        let root = try game([
            "se.pak": pack([("rain\\x.ogg", 70)]),
            "sys_5/se.pak": pack([("rain\\x.ogg", 71)]),
            "sys_5/cg.pak": pack([("sub\\y.bmp", 72)]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(size(root, "se\\rain\\x.ogg") == 71)
        #expect(size(root, "rain\\x.ogg") == 71)
        #expect(size(root, "sys_5\\se\\rain\\x.ogg") == 71)
        // Art in one stays the patch's own.
        #expect(size(root, "sys_5\\cg\\sub\\y.bmp") == 72)
        #expect(size(root, "sub\\y.bmp") == -1)
    }

    @Test func aPatchsLooseFilesOutrankItsArchives() throws {
        let root = try game([
            "sys_5.pak": pack([("scr\\x.txt", 80)]),
            "sys_5/scr.pak": pack([("x.txt", 81), ("y.txt", 82)]),
            "sys_5/scr/x.txt": Data(count: 83),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(size(root, "scr\\x.txt") == 83)
        #expect(size(root, "scr\\y.txt") == 82)
    }

    @Test func aLaterPatchWins() throws {
        let root = try game([
            "cfg.pak": pack([("init.ini", 50)]),
            "sys_2.pak": pack([("cfg\\init.ini", 52)]),
            "sys_3.pak": pack([("cfg\\init.ini", 53)]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(size(root, "cfg\\init.ini") == 53)
        #expect(size(root, "sys_2\\cfg\\init.ini") == 52)
    }
}
