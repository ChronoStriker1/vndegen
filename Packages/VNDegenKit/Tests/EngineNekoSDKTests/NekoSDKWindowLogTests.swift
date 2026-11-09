import CNekoSDK
import Foundation
import Testing
import TestSupport

// The log in the message window (`inwindow` under `screen log`), as Test Games 82 and 83 show it: the window's BackText
// button shows the line on screen in the log's colour first, then each press one line older, NextText one newer, and a
// click elsewhere brings the line in play back without going on (set 2, against 82's PC under Wine).
@Suite struct NekoSDKWindowLogTests {
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

    // A message window button: its sheet of five rows, kind, place, cells, the five cells' places, then its help.
    private func button(_ action: String, x: Int) -> String {
        "#inner \(action)\n" + ["obj\\b.bmp", "1", "\(x),600", "1,5", "0,0", "0,1", "0,2", "0,3", "0,4", "none", "0,0", "1,1"].joined(separator: "\n") + "\n##\n"
    }

    private func status(_ e: OpaquePointer) -> String { String(cString: vnneko_debug_status(e)) }

    // Whether any of the frame's text is in the log's colour, yellow.
    private func yellow(_ e: OpaquePointer) -> Bool {
        for _ in 0..<2 { _ = vnneko_step(e, 16) }
        var w: Int32 = 0, h: Int32 = 0, changed = false
        guard let px = vnneko_frame(e, &w, &h, &changed) else { return false }
        for i in stride(from: 0, to: Int(w) * Int(h) * 4, by: 4) where px[i] > 220 && px[i + 1] > 220 && px[i + 2] < 60 { return true }
        return false
    }

    private func click(_ e: OpaquePointer, _ x: Int, _ y: Int) {
        vnneko_mouse_move(e, Float(x) / 1280, Float(y) / 720)
        for _ in 0..<2 { _ = vnneko_step(e, 16) }
        vnneko_mouse_button(e, 0, true)
        vnneko_mouse_button(e, 0, false)
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
    }

    @Test func theWindowsButtonsStepThroughTheLogOneLineAPress() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\windowlogtest\nFirstFlow\tflow\\t.txt\nMsgLogColorR\t255\nMsgLogColorG\t255\nMsgLogColorB\t0\n".utf8),
            "cfg/msg_button.ini": Data((button("BackText", x: 0) + button("NextText", x: 100)).utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("b.bmp", bmp(50, 250))]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 5, next: 3, s: [2: "one"]),
                .init(op: 5, next: 4, s: [2: "two"]),
                .init(op: 5, next: 5, s: [2: "three"]),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<60 { _ = vnneko_step(e, 16) }
        // To the third line.
        for _ in 0..<2 {
            click(e, 640, 300)
            for _ in 0..<60 { _ = vnneko_step(e, 16) }
        }
        #expect(status(e).contains("s.txt #5 click"), "\(status(e))")
        #expect(!yellow(e))
        // The first press shows the line on screen in the log's colour, the next the line before it.
        click(e, 25, 625)
        #expect(status(e).hasSuffix(" log 1"), "\(status(e))")
        #expect(yellow(e))
        click(e, 25, 625)
        #expect(status(e).hasSuffix(" log 2"), "\(status(e))")
        click(e, 125, 625)
        #expect(status(e).hasSuffix(" log 1"), "\(status(e))")
        // A click elsewhere brings the line in play back and goes no further.
        click(e, 640, 300)
        #expect(!status(e).contains(" log "), "\(status(e))")
        #expect(status(e).contains("s.txt #5 click"), "\(status(e))")
        #expect(!yellow(e))
        // Back as far as the first line, and no further.
        for _ in 0..<5 { click(e, 25, 625) }
        #expect(status(e).hasSuffix(" log 3"), "\(status(e))")
    }
}
