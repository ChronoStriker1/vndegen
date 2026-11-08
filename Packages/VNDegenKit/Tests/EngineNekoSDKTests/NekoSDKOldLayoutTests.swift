import CNekoSDK
import Foundation
import Testing
import TestSupport

// The older layout's commands (core/oldlayout.cpp): a background at the top left, a standing picture at one of six places,
// halfway down and moved by parameters 2 and 3, each shown by the next update, and the deletes, as Test Game 83's PC saves
// place them.
@Suite struct NekoSDKOldLayoutTests {
    // A 24-bit BMP of one colour, `w` by `h`, as blue, green, red.
    private func bmp(_ w: Int, _ h: Int, _ bgr: [UInt8]) -> Data {
        let row = (w * 3 + 3) & ~3
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(54 + row * h)); put(0); put(54)
        put(40); put(UInt32(w)); put(UInt32(h))
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(UInt32(row * h)); put(0); put(0); put(0); put(0)
        var line = Data()
        for _ in 0..<w { line.append(contentsOf: bgr) }
        line.append(Data(repeating: 0, count: row - w * 3))
        for _ in 0..<h { d.append(line) }
        return d
    }

    private func pixel(_ e: OpaquePointer, _ x: Int, _ y: Int) -> [UInt8] {
        var w: Int32 = 0, h: Int32 = 0, changed = false
        guard let px = vnneko_frame(e, &w, &h, &changed) else { return [] }
        let at = (y * Int(w) + x) * 4
        return [px[at], px[at + 1], px[at + 2]]
    }

    @Test func placesSlotsAndDeletes() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("FirstFlow\tflow\\t.txt\n".utf8),
            "bg.pak": NekoSDKFlowFixture.archive([("g.bmp", bmp(4, 4, [0, 255, 0]))]),
            "sc.pak": NekoSDKFlowFixture.archive([("r.bmp", bmp(101, 81, [0, 0, 255]))]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 10, next: 3, s: [1: "bg\\g.bmp"]),
                // Place 3, its middle 400 left of the screen's, moved 5 right and 7 down.
                .init(op: 13, next: 4, p: [0: 3, 2: 5, 3: 7], s: [1: "sc\\r.bmp"]),
                .init(op: 9, next: 5),
                .init(op: 2, next: 6, p: [0: 1000]),
                .init(op: 14, next: 7, p: [0: 3]),
                .init(op: 12, next: 8),
                .init(op: 9, next: 9),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        // (1280 - 101) / 2 - 400 + 5 = 194, and (720 - 81) / 2 + 7 = 326.
        #expect(pixel(e, 0, 0) == [0, 255, 0])
        #expect(pixel(e, 194, 326) == [255, 0, 0])
        #expect(pixel(e, 294, 406) == [255, 0, 0])
        #expect(pixel(e, 193, 326) != [255, 0, 0])
        #expect(pixel(e, 194, 325) != [255, 0, 0])
        #expect(pixel(e, 295, 326) != [255, 0, 0])
        for _ in 0..<100 { _ = vnneko_step(e, 16) }
        #expect(pixel(e, 0, 0) != [0, 255, 0])
        #expect(pixel(e, 194, 326) != [255, 0, 0])
    }

    // Test Game 82 keeps its CGs in slot 102 (`layoutcgslot`), and a CG takes the background away, and a background the CG.
    @Test func aCGSlotOfItsOwnTakesTheBackgroundAway() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\layouttest\nFirstFlow\tflow\\t.txt\n".utf8),
            "bg.pak": NekoSDKFlowFixture.archive([("g.bmp", bmp(8, 8, [0, 255, 0]))]),
            "cg.pak": NekoSDKFlowFixture.archive([("r.bmp", bmp(4, 4, [0, 0, 255]))]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 10, next: 3, s: [1: "bg\\g.bmp"]),
                .init(op: 11, next: 4, s: [1: "cg\\r.bmp"]),
                .init(op: 9, next: 5),
                .init(op: 2, next: 6, p: [0: 1000]),
                .init(op: 10, next: 7, s: [1: "bg\\g.bmp"]),
                .init(op: 9, next: 8),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        // The CG over where the background was, and none of the background, the larger, past it.
        #expect(pixel(e, 0, 0) == [255, 0, 0])
        #expect(pixel(e, 6, 6) != [0, 255, 0])
        for _ in 0..<100 { _ = vnneko_step(e, 16) }
        #expect(pixel(e, 0, 0) == [0, 255, 0])
    }
}
