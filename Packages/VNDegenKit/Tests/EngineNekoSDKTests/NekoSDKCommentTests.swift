import CNekoSDK
import Foundation
import Testing
import TestSupport

// Test Game 96's viewer comments through the `yabai` part: command 110 loads a list, 112 plays a group of it, and each
// comment crosses the screen from right to left in 6.05 seconds, white with a black edge, as the PC draws them.
@Suite struct NekoSDKCommentTests {
    private func game(endAfter: Int32? = nil) throws -> URL {
        // A list of one group, `#p 0 40 80`, two lines; the scene loads it, plays the group and waits.
        let list = "//test\r\n#p 0 40 80\r\nABCDE\r\nFGHIJ\r\n\r\n##\r\n"
        return try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\commenttest\nFirstFlow\tflow\\t.txt\n".utf8),
            "cfg/com/c.ini": Data(list.utf8),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow(endAfter.map { ms in [
                .init(op: 0, next: 2),
                .init(op: 110, next: 3, s: [1: "cfg\\com\\c.ini"]),
                .init(op: 112, next: 4, p: [0: 0]),
                .init(op: 2, next: 5, p: [0: ms]),
                .init(op: 111, next: 6, p: [0: 0]),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ] } ?? [
                .init(op: 0, next: 2),
                .init(op: 110, next: 3, s: [1: "cfg\\com\\c.ini"]),
                .init(op: 112, next: 4, p: [0: 0]),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
    }

    // The columns holding white text with a black edge beside it, in the comments' rows.
    private func textColumns(_ e: OpaquePointer) -> [Int] {
        var w: Int32 = 0, h: Int32 = 0, changed = false
        guard let px = vnneko_frame(e, &w, &h, &changed) else { return [] }
        let width = Int(w)
        func at(_ x: Int, _ y: Int) -> [UInt8] { let i = (y * width + x) * 4; return [px[i], px[i + 1], px[i + 2]] }
        var columns = Set<Int>()
        for y in 10..<480 {
            for x in 3..<(width - 3) where at(x, y).allSatisfy({ $0 > 220 }) {
                if (-2...2).contains(where: { at(x + $0, y).allSatisfy { $0 < 40 } }) { columns.insert(x) }
            }
        }
        return columns.sorted()
    }

    @Test func aGroupsCommentsCrossFromRightToLeft() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<30 { _ = vnneko_step(e, 16) }
        let early = textColumns(e)
        #expect(!early.isEmpty)
        #expect((early.last ?? 0) > 1100, "\(early.last ?? -1)")
        // A second later the first comment is about (1280 + its width) / 6.05 pixels further left.
        for _ in 0..<62 { _ = vnneko_step(e, 16) }
        let later = textColumns(e)
        let moved = (early.first ?? 0) - (later.first ?? 0)
        #expect(moved > 180 && moved < 260, "moved \(moved)")
        // After seven seconds both have crossed and gone.
        for _ in 0..<440 { _ = vnneko_step(e, 16) }
        #expect(textColumns(e).isEmpty)
    }

    // Command 111 takes the comments away at once, those on their way too, as 96's PC does.
    @Test func endingTheCommentsTakesThemAway() throws {
        let root = try game(endAfter: 1000)
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<40 { _ = vnneko_step(e, 16) }
        #expect(!textColumns(e).isEmpty)
        for _ in 0..<40 { _ = vnneko_step(e, 16) }
        #expect(textColumns(e).isEmpty)
    }

    // 96's message window has a Comment button of its own (msg_button.ini's `#inner Comment`): with the table's
    // `inneralways` it shows with the pointer away from the window, and pressing it hides the comments and shows them again.
    @Test func theCommentButtonHidesAndShowsTheComments() throws {
        // A 24-bit BMP of five rows, 40 by 20 each, all magenta.
        let button: Data = {
            let w = 40, h = 100, row = w * 3
            var d = Data("BM".utf8)
            func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
            put(UInt32(54 + row * h)); put(0); put(54)
            put(40); put(UInt32(w)); put(UInt32(h))
            d.append(contentsOf: [1, 0, 24, 0])
            put(0); put(UInt32(row * h)); put(0); put(0); put(0); put(0)
            for _ in 0..<(w * h) { d.append(contentsOf: [255, 0, 255]) }
            return d
        }()
        let list = "#p 0 40 80\r\nABCDE\r\nFGHIJ\r\n##\r\n"
        let inner = ["obj\\b.bmp", "2", "1000,600", "1,5", "0,0", "0,1", "0,2", "0,3", "0,4", "none", "0,0", "1,1"]
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\commenttest\nFirstFlow\tflow\\t.txt\n".utf8),
            "cfg/com/c.ini": Data(list.utf8),
            "cfg/msg_button.ini": Data(("#inner Comment\n" + inner.joined(separator: "\n") + "\n##\n").utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("b.bmp", button)]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 110, next: 3, s: [1: "cfg\\com\\c.ini"]),
                .init(op: 112, next: 4, p: [0: 0]),
                .init(op: 5, next: 5, s: [2: "hello"]),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        // The pointer well away from the window.
        vnneko_mouse_move(e, 0.5, 0.3)
        for _ in 0..<40 { _ = vnneko_step(e, 16) }
        func buttonShows() -> Bool {
            var w: Int32 = 0, h: Int32 = 0, changed = false
            guard let px = vnneko_frame(e, &w, &h, &changed) else { return false }
            let at = (610 * Int(w) + 1020) * 4
            return px[at] > 200 && px[at + 1] < 60 && px[at + 2] > 200
        }
        #expect(buttonShows())
        // The comments move and the window's own text doesn't: with them showing the white text's columns change from
        // one moment to the next, and hidden they stay as they are.
        func moving() -> Bool {
            let before = textColumns(e)
            for _ in 0..<12 { _ = vnneko_step(e, 16) }
            return textColumns(e) != before
        }
        #expect(moving())
        #expect(vnneko_debug_press(e, "Comment"))
        for _ in 0..<3 { _ = vnneko_step(e, 16) }
        #expect(!moving())
        #expect(vnneko_debug_press(e, "Comment"))
        for _ in 0..<3 { _ = vnneko_step(e, 16) }
        #expect(moving())
    }

    // The Comment button's state, as 96's PC draws it: disabled until the scene loads a comment list, plain from then on
    // with no comment on screen, and lit once pressed. The sheet's rows are told apart by their blue.
    @Test func theCommentButtonIsDisabledUntilAListLoads() throws {
        let button: Data = {
            let w = 40, h = 100, row = w * 3
            var d = Data("BM".utf8)
            func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
            put(UInt32(54 + row * h)); put(0); put(54)
            put(40); put(UInt32(w)); put(UInt32(h))
            d.append(contentsOf: [1, 0, 24, 0])
            put(0); put(UInt32(row * h)); put(0); put(0); put(0); put(0)
            // Bottom row first: blue 20 for the top state, 180 for the fifth.
            for y in (0..<h).reversed() { for _ in 0..<w { d.append(contentsOf: [UInt8(40 * (y / 20) + 20), 0, 255]) } }
            return d
        }()
        let inner = ["obj\\b.bmp", "2", "1000,600", "1,5", "0,0", "0,1", "0,2", "0,3", "0,4", "none", "0,0", "1,1"]
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\commenttest\nFirstFlow\tflow\\t.txt\n".utf8),
            "cfg/com/c.ini": Data("#p 0 40 80\r\nABCDE\r\n##\r\n".utf8),
            "cfg/msg_button.ini": Data(("#inner Comment\n" + inner.joined(separator: "\n") + "\n##\n").utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("b.bmp", button)]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 5, next: 3, s: [2: "one"]),
                .init(op: 110, next: 4, s: [1: "cfg\\com\\c.ini"]),
                .init(op: 5, next: 5, s: [2: "two"]),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        vnneko_mouse_move(e, 0.5, 0.3)
        for _ in 0..<40 { _ = vnneko_step(e, 16) }
        func row() -> Int {
            for _ in 0..<3 { _ = vnneko_step(e, 16) }
            var w: Int32 = 0, h: Int32 = 0, changed = false
            guard let px = vnneko_frame(e, &w, &h, &changed) else { return -1 }
            let at = (610 * Int(w) + 1020) * 4
            return px[at] > 200 && px[at + 1] < 60 ? (Int(px[at + 2]) - 20) / 40 : -1
        }
        #expect(row() == 4)
        // A press does nothing while it's disabled.
        #expect(vnneko_debug_press(e, "Comment"))
        #expect(row() == 4)
        // On to the second line, past the list's load: no comment plays, and the button is plain.
        vnneko_mouse_button(e, 0, true)
        vnneko_mouse_button(e, 0, false)
        for _ in 0..<40 { _ = vnneko_step(e, 16) }
        #expect(row() == 0)
        #expect(vnneko_debug_press(e, "Comment"))
        #expect(row() == 2)
        #expect(vnneko_debug_press(e, "Comment"))
        #expect(row() == 0)
    }
}
