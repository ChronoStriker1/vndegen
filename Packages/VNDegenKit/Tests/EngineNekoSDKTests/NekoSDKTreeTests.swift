import CNekoSDK
import Foundation
import Testing
import TestSupport

// Test Game 96's confession tree (the `yabaitree` part): a press on a question of the current stage asks it, a press on a
// replay plays an asked one, and either leaves the chapter and how in the hub's variables, S 15110 and S 15111, as the
// hub reads them; the right button closes it with none.
@Suite struct NekoSDKTreeTests {
    private func game() throws -> URL {
        // Three stages of three questions for each heroine at truth 1, as 96's *_tree.ini name them.
        var tree = ""
        for rank in 1...3 { tree += "Level01Rank0\(rank)Num\t3\nLevel01Rank0\(rank)Ans\t0\n//Level01Rank0\(rank)Filename\t\tobj\\\\a\(rank).bmp\n//Level01Rank0\(rank)AfterFilename\tobj\\\\b\(rank).bmp\n" }
        return try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\treetest\n".utf8),
            "cfg/ayano_tree.ini": Data(tree.utf8),
            "cfg/misuzu_tree.ini": Data(tree.utf8),
            "cfg/rina_tree.ini": Data(tree.utf8),
            "obj.pak": NekoSDKFlowFixture.archive([]),
        ])
    }

    private func press(_ e: OpaquePointer, _ x: Int, _ y: Int, button: Int32 = 0) {
        vnneko_mouse_move(e, Float(x) / 1280, Float(y) / 720)
        for _ in 0..<2 { _ = vnneko_step(e, 16) }
        vnneko_mouse_button(e, button, true)
        vnneko_mouse_button(e, button, false)
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
    }

    private func status(_ e: OpaquePointer) -> String { String(cString: vnneko_debug_status(e)) }

    @Test func askingAQuestionLeavesItsChapter() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        #expect(status(e).contains("part screen"), "\(status(e))")
        // A solved stage's unasked question takes no press: stage 1, question 1.
        press(e, 508 + 130, 126 + 36)
        #expect(status(e).contains("part screen"), "\(status(e))")
        // Misuzu's stage 3, question 1: chapter 2000 + 20 + 1, to ask.
        press(e, 508 + 135, 396 + 44)
        #expect(vnneko_debug_variable(e, 1, 15110) == 2021)
        #expect(vnneko_debug_variable(e, 1, 15111) == 2)
        #expect(vnneko_debug_variable(e, 1, 15121) == 1)
    }

    @Test func aReplayLeavesItsChapterAndTheRightButtonNone() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        // Stage 2, question 2 was asked: its ▶ replays chapter 2000 + 10 + 2.
        press(e, 782 + 217 + 16, 261 + 23 + 16)
        #expect(vnneko_debug_variable(e, 1, 15110) == 2012)
        #expect(vnneko_debug_variable(e, 1, 15111) == 1)
    }

    @Test func theRightButtonClosesWithNone() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        // Ayano's tab first, then the right button.
        press(e, 842 + 59, 35 + 29)
        press(e, 640, 360, button: 1)
        #expect(vnneko_debug_variable(e, 1, 15110) == 0)
        #expect(vnneko_debug_variable(e, 1, 15108) == 0)
        #expect(vnneko_debug_variable(e, 1, 15121) == 1)
    }

    // A save made with the tree open brings it back open on a load, with the hub's variables, which the PC's save has no
    // room for, from VNDegen's lines beside it; and it still takes a press.
    @Test func aSaveWithTheTreeOpenLoadsItOpen() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        #expect(vnneko_debug_save(e, 3))
        // Leave by Ayano's tab and the right button, then load.
        press(e, 842 + 59, 35 + 29)
        press(e, 640, 360, button: 1)
        #expect(vnneko_debug_variable(e, 1, 15121) == 1)
        #expect(vnneko_debug_load(e, 3))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        #expect(status(e).contains("part screen"), "\(status(e))")
        #expect(vnneko_debug_variable(e, 1, 15102) == 3)
        #expect(vnneko_debug_variable(e, 1, 15134) == 6)
        #expect(vnneko_debug_variable(e, 1, 15121) == 0)
        // Misuzu again, as saved: her stage 3, question 1 asks chapter 2021.
        press(e, 508 + 135, 396 + 44)
        #expect(vnneko_debug_variable(e, 1, 15110) == 2021)
    }

    // A controller reaches the tree's controls: down from between stages 2 and 3 to the question below, which Confirm
    // presses where the pointer went.
    @Test func focusMovesToAQuestion() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        vnneko_mouse_move(e, Float(508 + 135) / 1280, 360.0 / 720)
        for _ in 0..<2 { _ = vnneko_step(e, 16) }
        #expect(vnneko_move_focus(e, 0, 1))
        for _ in 0..<2 { _ = vnneko_step(e, 16) }
        vnneko_mouse_button(e, 0, true)
        vnneko_mouse_button(e, 0, false)
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        #expect(vnneko_debug_variable(e, 1, 15110) == 2021)
    }
}
