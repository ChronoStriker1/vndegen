import CNekoSDK
import Foundation
import Testing
import TestSupport

// Scripts name a variable by scope and its place in macrolist.ini's list, which the PC keeps by name: Test Game 94's
// file lists GLB_PROLOGUE_OVER first, and its flows count it fifth, after the four endings. A global's place is
// parameter 1 of a condition or change and a scenario variable's parameter 2, as Test Game 88's flows show.
@Suite struct NekoSDKMacroListTests {
    private let macros = """
    #p Global
    GLB_PROLOGUE_OVER\t1000\t//comment
    GLB_ENDING_A\t\t51
    GLB_ENDING_B\t\t52
    GLB_TITLE_BG\t\t55
    ##
    #p Senario
    SENARIO_STATUS\t\t4000
    SENARIO_ROUTE_A\t\t100
    SENARIO_ROUTE_B\t\t101
    ##
    """

    // A game folder of loose files, with an empty archive, since a folder needs one to count as a NekoSDK game.
    private func game(_ files: [String: Data]) throws -> URL {
        let root = TestFolders.root.appendingPathComponent("nekosdk-macros-\(UUID().uuidString)")
        var all = files
        all["obj.pak"] = Data("NEKOPACK4A".utf8) + Data([4, 0, 0, 0, 0, 0, 0, 0])
        for (path, data) in all {
            let url = root.appendingPathComponent(path)
            try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try data.write(to: url)
        }
        return root
    }

    // A flow script of nodes given as (opcode, next, parameters), numbered from 1.
    private func flow(_ nodes: [(op: UInt32, next: UInt32, p: [Int: Int32])]) -> Data {
        var data = Data("NEKOSDK_ADVSCRIPT2".utf8) + Data([0])
        func put<T: FixedWidthInteger>(_ v: T) { data.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(nodes.count))
        for (i, node) in nodes.enumerated() {
            for v: UInt32 in [UInt32(i + 1), 1, 0, node.op] { put(v) }
            for k in 0..<32 { put(node.p[k] ?? 0) }
            put(node.next)
            for _ in 0..<16 { put(Int32(0)) }
            for _ in 0..<33 { put(UInt32(0)) }
        }
        return data
    }

    @Test func placesCountInNameOrder() throws {
        let root = try game(["cfg/macrolist.ini": Data(macros.utf8)])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect((0..<4).map { vnneko_debug_macro_number(root.path, 0, $0) } == [51, 52, 1000, 55])
        #expect((0..<3).map { vnneko_debug_macro_number(root.path, 1, $0) } == [100, 101, 4000])
        #expect(vnneko_debug_macro_number(root.path, 0, 4) == -1)
    }

    @Test func aChangeTakesAScenarioVariablesPlaceFromParameter2() throws {
        // Each change keeps a stale place for the other scope, as the editor does.
        let root = try game([
            "cfg/init.ini": Data("FirstFlow\tflow\\t.txt\n".utf8),
            "cfg/macrolist.ini": Data(macros.utf8),
            "flow/t.txt": flow([
                (0, 2, [:]),
                (6, 3, [0: 1, 1: 3, 2: 1, 4: 2, 5: 5]),
                (6, 4, [0: 0, 1: 2, 2: 2, 4: 2, 5: 9]),
                (2, 0, [:]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let engine: OpaquePointer? = root.path.withCString { rootPath in
            data.path.withCString { dataPath in
                var launch = VNNekoLaunch(gameRoot: rootPath, dataDir: dataPath, logPath: nil, fontDir: nil, movies: nil)
                return vnneko_start(&launch)
            }
        }
        let e = try #require(engine)
        defer { vnneko_stop(e) }
        for _ in 0..<20 where vnneko_step(e, 16) {}
        #expect(vnneko_debug_variable(e, 1, 101) == 5)
        #expect(vnneko_debug_variable(e, 1, 100) == 0)
        #expect(vnneko_debug_variable(e, 0, 1000) == 9)
    }
}
