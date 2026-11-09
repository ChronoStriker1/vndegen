import CNekoSDK
import Foundation
import Testing
import TestSupport

// Test Game 82's save cards: the chapter's title picked by the save's S5000 (`slotchaptervar`), each part of the date at a place
// of its own with a blank for a leading zero (`datefield`), a frame around the thumbnail (`slotthumbframe`) and card
// numbers 15 apart from 14-wide digits (`slotdigits`' step).
@Suite struct NekoSDKCardFieldTests {
    // A BMP of w by h whose pixel at x, y is color(x, y), as blue, green, red.
    private func bmp(_ w: Int, _ h: Int, _ color: (Int, Int) -> [UInt8]) -> Data {
        let row = (w * 3 + 3) & ~3
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(54 + row * h)); put(0); put(54)
        put(40); put(UInt32(w)); put(UInt32(h))
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(UInt32(row * h)); put(0); put(0); put(0); put(0)
        for y in (0..<h).reversed() {
            var line = Data()
            for x in 0..<w { line.append(contentsOf: color(x, y)) }
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

    @Test func aCardDrawsItsChapterDateFrameAndNumberWhereTheTableSays() throws {
        // The card is blue; each digit's red is 20 times it plus 10; each chapter cell's green is 60 times it plus 30.
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\cardtest\nFirstFlow\tflow\\t.txt\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([
                ("c.bmp", bmp(482, 555) { _, _ in [200, 0, 0] }),
                ("d.bmp", bmp(140, 20) { x, _ in [0, 0, UInt8(20 * (x / 14) + 10)] }),
                ("n.bmp", bmp(342, 22) { x, _ in [0, UInt8(60 * (x / 114) + 30), 0] }),
                ("f.bmp", bmp(158, 90) { _, _ in [0, 250, 250] }),
            ]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                // S5000 = 2.
                .init(op: 6, next: 3, p: [0: 1, 1: 0, 2: 5000, 4: 2, 5: 2]),
                .init(op: 1, next: 0, s: [1: "scr\\s.txt"]),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([.init(op: 0, next: 2), .init(op: 2, next: 0, p: [0: 100_000])]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_jump(first, "flow\\t.txt", 0))
        for _ in 0..<10 { _ = vnneko_step(first, 16) }
        #expect(vnneko_debug_variable(first, 1, 5000) == 2)
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)

        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_press(e, "load"))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        let now = Calendar.current.dateComponents([.year, .month, .day], from: Date())
        func digitAt(_ x: Int, _ y: Int) -> Int? {
            let p = pixel(e, x, y)
            return p == [0, 0, 200] ? nil : (Int(p[0]) - 10) / 20
        }
        // Card 1 at 60, 99: the number 001, 15 apart.
        #expect([229, 244, 259].map { digitAt(60 + $0 + 7, 99 + 6 + 10) } == [0, 0, 1])
        // The year's four digits from 251, 28.
        let year = String(now.year!).compactMap { $0.wholeNumberValue }
        #expect((0..<4).map { digitAt(60 + 251 + 15 * $0 + 7, 99 + 28 + 10) } == year.map { Optional($0) })
        // The month and day right-aligned in two places, the first blank below 10.
        for (value, x) in [(now.month!, 347), (now.day!, 406)] {
            #expect(digitAt(60 + x + 15 + 7, 99 + 28 + 10) == value % 10)
            #expect(digitAt(60 + x + 7, 99 + 28 + 10) == (value >= 10 ? value / 10 : nil))
        }
        // The chapter from S5000: cell 2, green 150.
        #expect(pixel(e, 60 + 350 + 5, 99 + 6 + 5) == [0, 150, 0])
        // The frame's corner, outside the thumbnail.
        #expect(pixel(e, 60 + 27, 99 + 8) == [250, 250, 0])
        // Card 2 has no save: no frame and no chapter.
        #expect(pixel(e, 570 + 27, 99 + 8) == [0, 0, 200])
        #expect(pixel(e, 570 + 350 + 5, 99 + 6 + 5) == [0, 0, 200])
    }
}
