import CNekoSDK
import Foundation
import TestSupport

// A NekoSDK game for flow tests: its files loose in a patch folder, the one kind of folder the runtime reads loose, and an
// empty archive, since a folder needs one to count as a game.
enum NekoSDKFlowFixture {
    struct Node {
        var op: UInt32
        var next: UInt32
        var otherwise: UInt32 = 0
        var p: [Int: Int32] = [:]
        var s: [Int: String] = [:]
    }

    static func game(_ files: [String: Data]) throws -> URL {
        let root = TestFolders.root.appendingPathComponent("nekosdk-flow-\(UUID().uuidString)")
        var all = files
        if all["obj.pak"] == nil { all["obj.pak"] = Data("NEKOPACK4A".utf8) + Data([4, 0, 0, 0, 0, 0, 0, 0]) }
        for (path, data) in all {
            let url = root.appendingPathComponent(path.hasSuffix(".pak") ? path : "sys_1/" + path)
            try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try data.write(to: url)
        }
        return root
    }

    // A flow script of these nodes, numbered from 1.
    static func flow(_ nodes: [Node]) -> Data {
        var data = Data("NEKOSDK_ADVSCRIPT2".utf8) + Data([0])
        func put<T: FixedWidthInteger>(_ v: T) { data.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(nodes.count))
        for (i, node) in nodes.enumerated() {
            for v: UInt32 in [UInt32(i + 1), 1, 0, node.op] { put(v) }
            for k in 0..<32 { put(node.p[k] ?? 0) }
            put(node.next)
            put(node.otherwise)
            for _ in 1..<16 { put(Int32(0)) }
            for k in 0..<33 {
                // Each string is its length then its bytes, NUL included, in CP932, as the editor writes them.
                let text = (node.s[k] ?? "").data(using: .shiftJIS) ?? Data()
                put(UInt32(text.isEmpty ? 0 : text.count + 1))
                if !text.isEmpty { data.append(text + Data([0])) }
            }
        }
        return data
    }

    // Starts the game with its user data in `data`.
    static func start(_ root: URL, data: URL) -> OpaquePointer? {
        let gamePath: String = root.path, dataDirPath: String = data.path
        return gamePath.withCString { rootPath in
            dataDirPath.withCString { dataPath in
                var launch = VNNekoLaunch(gameRoot: rootPath, dataDir: dataPath, logPath: nil, fontDir: nil, movies: nil)
                let engine = vnneko_start(&launch)
                if let engine { vnneko_debug_host_taps(engine, true) }
                return engine
            }
        }
    }

    // Starts a game whose story is this one flow, with this macro list, and runs it to its end.
    static func run(_ nodes: [Node], macros: String, _ check: (OpaquePointer) -> Void) throws {
        let root = try game([
            "cfg/init.ini": Data("FirstFlow\tflow\\t.txt\n".utf8),
            "cfg/macrolist.ini": macros.data(using: .shiftJIS) ?? Data(),
            "flow/t.txt": flow(nodes),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        guard let e = start(root, data: root.appendingPathComponent("data")) else { throw CocoaError(.featureUnsupported) }
        defer { vnneko_stop(e) }
        for _ in 0..<20 where vnneko_step(e, 16) {}
        check(e)
    }

    // A file packed as the PC packs cfg.sav: a zlib stream of stored blocks with its first 4 bytes XORed with a key from
    // the packed size, then the plain size.
    static func packed(_ plain: Data) -> Data {
        var z = Data([0x78, 0x01])
        var at = 0
        repeat {
            let count = min(65535, plain.count - at)
            z.append(at + count == plain.count ? 1 : 0)
            z.append(contentsOf: withUnsafeBytes(of: UInt16(count).littleEndian, Array.init))
            z.append(contentsOf: withUnsafeBytes(of: (~UInt16(count)).littleEndian, Array.init))
            z.append(plain[plain.startIndex + at..<plain.startIndex + at + count])
            at += count
        } while at < plain.count
        var a: UInt32 = 1, b: UInt32 = 0
        for byte in plain { a = (a + UInt32(byte)) % 65521; b = (b + a) % 65521 }
        z.append(contentsOf: withUnsafeBytes(of: (b << 16 | a).bigEndian, Array.init))
        z.append(contentsOf: withUnsafeBytes(of: UInt32(plain.count).littleEndian, Array.init))
        var key = UInt32(z.count / 8 + 0x22)
        for i in 0..<4 { z[i] ^= UInt8(key & 0xff); key <<= 3 }
        return z
    }

    // A NEKOPACK4A archive of these entries: the index lists each name with its offset and size XORed with the sum of
    // the name's signed bytes, and each entry is packed as cfg.sav is. Art has to come from an archive, since a patch
    // folder's art is used only as its art set.
    static func archive(_ entries: [(String, Data)]) -> Data { archive(raw: entries.map { (Array($0.0.utf8), $0.1) }) }

    // The same with each name as its bytes, for names in the games' own code page.
    static func archive(raw entries: [([UInt8], Data)]) -> Data {
        var index = Data(), body = Data()
        let header = 14 + entries.reduce(0) { $0 + 4 + $1.0.count + 1 + 8 }
        func put(_ v: UInt32, _ d: inout Data) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        for (name, plain) in entries {
            let entry = packed(plain), bytes = name + [0]
            let key = bytes.reduce(UInt32(0)) { $0 &+ UInt32(bitPattern: Int32(Int8(bitPattern: $1))) }
            put(UInt32(bytes.count), &index)
            index.append(contentsOf: bytes)
            put(UInt32(header + body.count) ^ key, &index)
            put(UInt32(entry.count) ^ key, &index)
            body.append(entry)
        }
        var data = Data("NEKOPACK4A".utf8)
        put(UInt32(index.count), &data)
        return data + index + body
    }

    // The plain bytes of a packed file.
    static func unpacked(_ file: Data) throws -> Data {
        var z = file
        var key = UInt32(z.count / 8 + 0x22)
        for i in 0..<4 { z[i] ^= UInt8(key & 0xff); key <<= 3 }
        // Past the zlib header, up to the Adler checksum and the size: a raw deflate stream.
        let deflate = z.subdata(in: 2..<(z.count - 8))
        return try (deflate as NSData).decompressed(using: .zlib) as Data
    }
}
