import CNekoSDK
import Foundation
import Testing
import TestSupport

// Test Game 82's choices take their options from columns of one sheet of all of them (`choice ... cells`, parameters 14
// on, with `choicecolumns`), and its box closes after a press, the chosen option pressed, before the story goes on.
@Suite struct NekoSDKChoiceColumnsTests {
    // A sheet of 4 columns and 5 rows of 20 by 10 cells, each cell's red its column and green its row.
    private func sheet() -> Data {
        let w = 80, h = 50, row = (w * 3 + 3) & ~3
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(54 + row * h)); put(0); put(54)
        put(40); put(UInt32(w)); put(UInt32(h))
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(UInt32(row * h)); put(0); put(0); put(0); put(0)
        // Rows bottom up, as BMPs keep them.
        for y in (0..<h).reversed() {
            var line = Data()
            for x in 0..<w { line.append(contentsOf: [0, UInt8(40 * (y / 10) + 20), UInt8(50 * (x / 20) + 30)]) }
            line.append(Data(repeating: 0, count: row - w * 3))
            d.append(line)
        }
        return d
    }

    private func pixel(_ e: OpaquePointer, _ x: Int, _ y: Int) -> [UInt8] {
        var w: Int32 = 0, h: Int32 = 0, changed = false
        guard let px = vnneko_frame(e, &w, &h, &changed) else { return [] }
        let at = (y * Int(w) + x) * 4
        return [px[at], px[at + 1], px[at + 2]]
    }

    @Test func optionsComeFromTheirColumnsAndTheBoxCloses() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\layouttest\nFirstFlow\tflow\\t.txt\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("m.bmp", sheet())]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                // Two options, from columns 3 and 1.
                .init(op: 3, next: 3, p: [0: 1, 1: 1, 10: 4, 11: 4, 12: 4, 13: 4, 14: 2, 15: 3, 16: 1], s: [1: "obj\\m.bmp"]),
                .init(op: 4, next: 4, otherwise: 5, p: [0: 1]),
                .init(op: 2, next: 0),
                .init(op: 1, next: 0, s: [1: "scr\\s.txt"]),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([.init(op: 0, next: 2), .init(op: 2, next: 0, p: [0: 100_000])]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_jump(e, "flow\\t.txt", 0))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        #expect(vnneko_debug_choice_count(e) == 2)
        // Stacked in the middle: x (1280 - 20) / 2 = 630, from y (720 - 20) / 2 = 350, 10 apart.
        #expect(pixel(e, 635, 355) == [180, 20, 0])
        #expect(pixel(e, 635, 365) == [80, 20, 0])
        vnneko_debug_choose(e, 1)
        _ = vnneko_step(e, 16)
        // Pressed: row 2 of its column (green 100 against row 0's 20), at about its place while the box is still nearly full
        // size, already a little faded.
        let pressed = pixel(e, 640, 365)
        #expect(pressed.count == 3 && pressed[1] > 80 && pressed[1] > pressed[0] && pressed[2] == 0, "\(pressed)")
        for _ in 0..<20 { _ = vnneko_step(e, 16) }
        #expect(pixel(e, 635, 365)[1] < 60)
        #expect(String(cString: vnneko_debug_status(e)).hasPrefix("sys_1\\scr\\s.txt") || String(cString: vnneko_debug_status(e)).hasPrefix("scr\\s.txt"))
    }
}
