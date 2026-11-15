import CNekoSDK
import Foundation
import Testing
import TestSupport

@Suite struct NekoSDKFlashTests {
    private func red(_ e: OpaquePointer) -> Int {
        var w: Int32 = 0, h: Int32 = 0, changed = false
        guard let px = vnneko_frame(e, &w, &h, &changed) else { return -1 }
        return Int(px[(Int(h / 2) * Int(w) + Int(w / 2)) * 4])
    }

    private func run(layer: Int32) throws -> [Int] {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("FirstFlow\tflow\\t.txt\n".utf8),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 50, next: 3, p: [0: 200, 1: 255, 5: 200, 7: 400, 10: layer]),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_jump(e, "flow\\t.txt", 0))
        _ = vnneko_step(e, 1)
        var levels: [Int] = []
        var elapsed = 0
        for at in [100, 300, 600, 900] {
            while elapsed < at {
                _ = vnneko_step(e, 10)
                elapsed += 10
            }
            levels.append(red(e))
        }
        return levels
    }

    @Test func aFlashFadesInHoldsAndFadesOut() throws {
        let levels = try run(layer: 10000)
        #expect(abs(levels[0] - 128) <= 24, "\(levels)")
        #expect(levels[1] >= 250, "\(levels)")
        #expect(abs(levels[2] - 128) <= 24, "\(levels)")
        #expect(levels[3] <= 2, "\(levels)")
    }

    @Test func aFlashWithParameter10At0ShowsNothing() throws {
        #expect(try run(layer: 0).allSatisfy { $0 <= 2 })
    }
}
