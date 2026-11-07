import CNekoSDK
import Foundation
import TestSupport

// A NekoSDK game of loose files for flow tests, with an empty archive, since a folder needs one to count as a game.
enum NekoSDKFlowFixture {
    struct Node {
        var op: UInt32
        var next: UInt32
        var otherwise: UInt32 = 0
        var p: [Int: Int32] = [:]
    }

    static func game(_ files: [String: Data]) throws -> URL {
        let root = TestFolders.root.appendingPathComponent("nekosdk-flow-\(UUID().uuidString)")
        var all = files
        all["obj.pak"] = Data("NEKOPACK4A".utf8) + Data([4, 0, 0, 0, 0, 0, 0, 0])
        for (path, data) in all {
            let url = root.appendingPathComponent(path)
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
            for _ in 0..<33 { put(UInt32(0)) }
        }
        return data
    }

    // Starts a game whose story is this one flow, with this macro list, and runs it to its end.
    static func run(_ nodes: [Node], macros: String, _ check: (OpaquePointer) -> Void) throws {
        let root = try game([
            "cfg/init.ini": Data("FirstFlow\tflow\\t.txt\n".utf8),
            "cfg/macrolist.ini": Data(macros.utf8),
            "flow/t.txt": flow(nodes),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let engine: OpaquePointer? = root.path.withCString { rootPath in
            data.path.withCString { dataPath in
                var launch = VNNekoLaunch(gameRoot: rootPath, dataDir: dataPath, logPath: nil, fontDir: nil, movies: nil)
                return vnneko_start(&launch)
            }
        }
        guard let e = engine else { throw CocoaError(.featureUnsupported) }
        defer { vnneko_stop(e) }
        for _ in 0..<20 where vnneko_step(e, 16) {}
        check(e)
    }
}
