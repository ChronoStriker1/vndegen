import CNekoSDK
import Foundation
import Testing
import TestSupport

// A choice's count is its parameter 0 and one, in every game's flows: Test Game 94's two-option choices give 1 in
// parameter 1, where most games give the count again.
@Suite struct NekoSDKChoiceTests {
    // A 24-bit BMP of one colour, `w` by `h`.
    private func bmp(_ w: Int, _ h: Int) -> Data {
        let row = (w * 3 + 3) & ~3
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(54 + row * h)); put(0); put(54)
        put(40); put(UInt32(w)); put(UInt32(h))
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(UInt32(row * h)); put(0); put(0); put(0); put(0)
        d.append(Data(repeating: 200, count: row * h))
        return d
    }

    @Test func aChoiceCountsParameter0AndOne() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("FirstFlow\tflow\\t.txt\n".utf8),
            "cfg/macrolist.ini": Data("#p Global\nGLB_A\t1000\n##\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("n001_000.bmp", bmp(20, 10))]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 3, next: 3, p: [0: 1, 1: 1], s: [1: "obj\\n001_000.bmp"]),
                .init(op: 2, next: 0),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_jump(e, "flow\\t.txt", 0))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        #expect(vnneko_debug_choice_count(e) == 2)
    }
}
