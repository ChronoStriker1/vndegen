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
}
