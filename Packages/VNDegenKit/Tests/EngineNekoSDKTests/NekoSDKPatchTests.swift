import CNekoSDK
import Foundation
import Testing

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

    private func game(_ files: [String: Data]) throws -> URL {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("nekosdk-patch-\(UUID().uuidString)")
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
