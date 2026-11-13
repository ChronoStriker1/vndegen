import CYuRIS
import Foundation
import Testing
import TestSupport

@Suite struct YuRISDamagedFileTests {
    private static func murmur2(_ data: [UInt8]) -> UInt32 {
        let m: UInt32 = 0x5bd1_e995
        var h = UInt32(truncatingIfNeeded: data.count)
        var i = 0
        while data.count - i >= 4 {
            var k = UInt32(data[i]) | UInt32(data[i + 1]) << 8 | UInt32(data[i + 2]) << 16 | UInt32(data[i + 3]) << 24
            k &*= m
            k ^= k >> 24
            k &*= m
            h &*= m
            h ^= k
            i += 4
        }
        let rest = data.count - i
        if rest == 3 { h ^= UInt32(data[i + 2]) << 16 }
        if rest >= 2 { h ^= UInt32(data[i + 1]) << 8 }
        if rest >= 1 {
            h ^= UInt32(data[i])
            h &*= m
        }
        h ^= h >> 13
        h &*= m
        h ^= h >> 15
        return h
    }

    private static func le<T: FixedWidthInteger>(_ v: T) -> [UInt8] { withUnsafeBytes(of: v.littleEndian, Array.init) }

    private static func bytes(_ parts: [[UInt8]]) -> [UInt8] { parts.flatMap { $0 } }

    static func archive(name: String, size: UInt32, csize: UInt32, packed: Bool = false, payload: [UInt8]) -> Data {
        let nameBytes = Array(name.utf8)
        func entry(offset: UInt64) -> [UInt8] {
            bytes([le(murmur2(nameBytes)), [UInt8(nameBytes.count) ^ 0xff], nameBytes, [0, packed ? 1 : 0],
                   le(size), le(csize), le(offset), le(UInt32(0))])
        }
        let dirSize = 32 + entry(offset: 0).count
        let head = bytes([Array("YPF".utf8), [0], le(UInt32(500)), le(UInt32(1)), le(UInt32(dirSize)), [UInt8](repeating: 0, count: 16)])
        return Data(bytes([head, entry(offset: UInt64(dirSize)), payload]))
    }

    static func game(_ files: [String: Data]) throws -> URL {
        let root = TestFolders.root.appending(path: "yuris-damaged-\(UUID().uuidString)")
        for (path, data) in files {
            let url = root.appending(path: path)
            try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try data.write(to: url)
        }
        return root
    }

    static func open(_ root: URL) -> (opened: Bool, error: String) {
        let save = root.appending(path: "save")
        var err = [CChar](repeating: 0, count: 512)
        let opened = root.path.withCString { gameDir in
            save.path.withCString { saveDir in
                var config = VNYConfig(gameDir: gameDir, saveDir: saveDir, logPath: nil, audioRate: 44100, startTime: 1_767_225_600)
                let game = vny_open(&config, &err, err.count)
                if let game { vny_close(game) }
                return game != nil
            }
        }
        return (opened, String(cString: err))
    }

    @Test func refusesAnArchiveEntryLargerThanTheArchive() throws {
        let payload = [UInt8](repeating: 0x41, count: 4096)
        for csize: UInt32 in [0xFFFF_FFFF, 0xFFFF_F000, 4097] {
            let root = try Self.game(["ysbin.ypf": Self.archive(name: "ysbin/ysc.ybn", size: csize, csize: csize, payload: payload)])
            let result = Self.open(root)
            #expect(!result.opened)
            #expect(result.error.contains("ysc.ybn"))
        }
    }

    @Test func refusesAPackedEntryThatClaimsMoreThanZlibCanExpand() throws {
        let root = try Self.game(["ysbin.ypf": Self.archive(name: "ysbin/ysc.ybn", size: 0xFFFF_FFF0, csize: 16, packed: true, payload: [UInt8](repeating: 0x78, count: 16))])
        let result = Self.open(root)
        #expect(!result.opened)
        #expect(result.error.contains("ysc.ybn"))
    }

    static func project(list: UInt32 = 1, labels: UInt32 = 0, scriptCount: UInt32 = 0) -> [String: Data] {
        let words: [UInt32] = [474, scriptCount, 4, 12, 0, 0, 0]
        return [
            "ysbin/ysc.ybn": Data(bytes([Array("YSCM".utf8), le(UInt32(0)), le(UInt32(0)), le(UInt32(0))])),
            "ysbin/yst_list.ybn": Data(bytes([Array("YSTL".utf8), le(UInt32(0)), le(list), le(UInt32(0)), le(UInt32(0)), [UInt8](repeating: 0, count: 20)])),
            "ysbin/ysl.ybn": Data(bytes([Array("YSLB".utf8), le(UInt32(0)), le(labels), [UInt8](repeating: 0, count: 1024)])),
            "ysbin/ysv.ybn": Data(bytes([Array("YSVR".utf8), [UInt8](repeating: 0, count: 6)])),
            "ysbin/yst00000.ybn": Data(bytes([Array("YSTB".utf8), words.flatMap { le($0) }, [UInt8](repeating: 0, count: 16)])),
        ]
    }

    @Test func refusesProjectCountsLargerThanTheirFiles() throws {
        let cases: [([String: Data], String)] = [
            (Self.project(list: 0xFFFF_FFFF), "yst_list.ybn"),
            (Self.project(labels: 0x7FFF_FFFF), "ysl.ybn"),
            (Self.project(scriptCount: 0x4000_0001), "system_start.yst"),
        ]
        for (files, file) in cases {
            let result = Self.open(try Self.game(files))
            #expect(!result.opened)
            #expect(result.error.contains(file))
        }
    }
}
