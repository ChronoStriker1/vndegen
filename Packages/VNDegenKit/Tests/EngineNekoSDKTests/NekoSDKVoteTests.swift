import CNekoSDK
import Foundation
import Testing
import TestSupport

// Test Game 96's vote (the `yabaivote` part): the pick screen takes a heroine, the vote runs its length from
// vote_base.ini, and the screen finishes with 1 when the vote elects the heroine picked, 0 when not; スキップ ends the vote
// at once, and the pick screen's 自白内容確認 finishes with 9 for the table to open the tree.
@Suite struct NekoSDKVoteTests {
    private func game() throws -> URL {
        // Vote 1: two seconds, Ayano elected; its script moves Misuzu and gives Ayano a line.
        let base = """
            AllTime01\t\t120\t// 残り時間
            BehaviorFilename01\tcfg\\\\m.ini
            VoteComment01\t\tcfg\\\\c.ini
            ElectedNum01\t\t1
            """
        let script = """
            0,0,com_stop,0
            0,0,comment,0
            0,MIS,face,4
            10,MIS,down_move,L,0.8
            20,AYA,message,1,B,N,0,0,FADE,obj\\\\o016_000.bmp,voice\\aya5_0000.ogg
            30,AYA.face_shake,S_X,2,10
            """
        return try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\votetest\n".utf8),
            "cfg/vote_base.ini": Data(base.utf8),
            "cfg/m.ini": Data(script.utf8),
            "cfg/c.ini": Data("#p 0 30 90\nABC\n##\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([]),
        ])
    }

    private func status(_ e: OpaquePointer) -> String { String(cString: vnneko_debug_status(e)) }

    private func steps(_ e: OpaquePointer, _ count: Int) { for _ in 0..<count { _ = vnneko_step(e, 16) } }

    private func press(_ e: OpaquePointer, _ x: Int, _ y: Int) {
        vnneko_mouse_move(e, Float(x) / 1280, Float(y) / 720)
        steps(e, 2)
        vnneko_mouse_button(e, 0, true)
        vnneko_mouse_button(e, 0, false)
        steps(e, 2)
    }

    private func start(_ root: URL) throws -> OpaquePointer {
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        // Past the pick screen's fade, during which presses don't count.
        steps(e, 40)
        #expect(status(e).contains("part screen"), "\(status(e))")
        return e
    }

    @Test func pickingTheOneElectedGivesOne() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try start(root)
        defer { vnneko_stop(e) }
        // Ayano's panel, in the middle.
        press(e, 636, 200)
        // 9.9 seconds before the vote's clock starts, its two seconds, and two more on 投票終了.
        steps(e, 400)
        #expect(status(e).contains("part screen"), "\(status(e))")
        #expect(vnneko_debug_variable(e, 1, 15141) == 0)
        steps(e, 500)
        #expect(vnneko_debug_variable(e, 1, 15140) == 1, "\(status(e))")
        #expect(vnneko_debug_variable(e, 1, 15141) == 1)
    }

    @Test func pickingAnotherGivesZeroAndSkipEndsTheVote() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try start(root)
        defer { vnneko_stop(e) }
        // Misuzu's panel, on the left; then スキップ.
        press(e, 150, 500)
        steps(e, 30)
        #expect(vnneko_debug_variable(e, 1, 15141) == 0)
        press(e, 1139 + 50, 25 + 20)
        steps(e, 5)
        #expect(vnneko_debug_variable(e, 1, 15141) == 1, "\(status(e))")
        #expect(vnneko_debug_variable(e, 1, 15140) == 0)
    }

    @Test func theTreeButtonGivesNine() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try start(root)
        defer { vnneko_stop(e) }
        press(e, 12 + 95, 581 + 47)
        steps(e, 5)
        #expect(vnneko_debug_variable(e, 1, 15140) == 9)
    }
}
