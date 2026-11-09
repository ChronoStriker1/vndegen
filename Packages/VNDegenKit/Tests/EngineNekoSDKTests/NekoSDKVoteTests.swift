import CNekoSDK
import Foundation
import Testing
import TestSupport

// Test Game 96's vote (the `yabaivote` part): the pick screen takes a heroine, the vote runs its length from
// vote_base.ini, and the screen finishes with 1 when the vote elects the heroine picked, 0 when not; スキップ ends the vote
// at once, and the pick screen's 自白内容確認 finishes with 9 for the table to open the tree. The game saves in Test Game 96's
// layout: a save at the pick or during the vote comes back to the pick, a PC save there too, by the PC's mode in S 105.
@Suite struct NekoSDKVoteTests {
    // A 24-bit BMP of one pixel.
    private var pixel: Data {
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(58); put(0); put(54)
        put(40); put(1); put(1)
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(4); put(0); put(0); put(0); put(0)
        d.append(contentsOf: [10, 20, 30, 0])
        return d
    }

    // A second of silence as a WAV, 8,000 16-bit samples.
    private var sound: Data {
        var d = Data("RIFF".utf8)
        func put32(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        func put16(_ v: UInt16) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put32(36 + 16_000)
        d.append(Data("WAVEfmt ".utf8))
        put32(16); put16(1); put16(1); put32(8_000); put32(16_000); put16(2); put16(16)
        d.append(Data("data".utf8))
        put32(16_000)
        d.append(Data(count: 16_000))
        return d
    }

    // A 24-bit BMP of one colour, as blue, green, red.
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

    private func game(panels: Bool = false, faces: Bool = false) throws -> URL {
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
            // The panels as one-pixel pictures, as a replacement could be: far smaller than the place a press lands.
            // And Misuzu's faces as a sheet of green, ten cells of 258 by 235.
            "obj.pak": NekoSDKFlowFixture.archive((panels ? ["n004_000.bmp", "n004_001.bmp", "n004_002.bmp"].map { ($0, pixel) } : [])
                + (faces ? [("o014_000.bmp", bmp(2580, 235, [0, 255, 0]))] : [])),
            "voice.pak": NekoSDKFlowFixture.archive([("aya5_0000.ogg", sound)]),
        ])
    }

    private func saves(_ root: URL) -> URL { root.appendingPathComponent("data/AppData/Roaming/vndegen/votetest/save") }

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

    // A panel's picture too small to hold the place pressed leaves its whole rectangle to press.
    @Test func aPanelSmallerThanItsPlaceStillTakesThePress() throws {
        let root = try game(panels: true)
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try start(root)
        defer { vnneko_stop(e) }
        press(e, 636, 200)
        press(e, 1139 + 50, 25 + 20)
        steps(e, 5)
        #expect(vnneko_debug_variable(e, 1, 15140) == 1, "\(status(e))")
        #expect(vnneko_debug_variable(e, 1, 15141) == 1)
    }

    // スキップ ends the vote's voices with it, so none runs on into the story.
    @Test func skipStopsTheVotesVoice() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try start(root)
        defer { vnneko_stop(e) }
        press(e, 150, 500)
        // Past the 9.9 seconds before the vote's clock and the 20 frames to Ayano's line.
        steps(e, 660)
        #expect(vnneko_debug_sound_playing(e, 2, 1))
        press(e, 1139 + 50, 25 + 20)
        steps(e, 5)
        #expect(vnneko_debug_variable(e, 1, 15141) == 1, "\(status(e))")
        #expect(!vnneko_debug_sound_playing(e, 2, 1))
        // And the story is in a scene again, by the PC's mode.
        #expect(vnneko_debug_variable(e, 1, 105) == 4)
    }

    // A save at the pick comes back to the pick with its truth, and the vote from there gives its result as before.
    @Test func aSaveAtThePickLoadsThePick() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try start(root)
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_variable(e, 1, 105) == 8)
        #expect(vnneko_debug_save(e, 1))
        // Away through the tree's button, then back by the load.
        press(e, 12 + 95, 581 + 47)
        steps(e, 5)
        #expect(vnneko_debug_variable(e, 1, 15141) == 1)
        #expect(vnneko_debug_load(e, 1))
        steps(e, 40)
        #expect(status(e).contains("part screen"), "\(status(e))")
        #expect(vnneko_debug_variable(e, 1, 15141) == 0)
        press(e, 636, 200)
        press(e, 1139 + 50, 25 + 20)
        steps(e, 5)
        #expect(vnneko_debug_variable(e, 1, 15140) == 1, "\(status(e))")
        #expect(vnneko_debug_variable(e, 1, 15141) == 1)
    }

    // A save during the vote comes back to the pick, where the PC's saves of a vote are: the pick is made again.
    @Test func aSaveDuringTheVoteLoadsThePick() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try start(root)
        defer { vnneko_stop(e) }
        press(e, 150, 500)
        steps(e, 30)
        #expect(vnneko_debug_save(e, 2))
        press(e, 1139 + 50, 25 + 20)
        steps(e, 5)
        #expect(vnneko_debug_variable(e, 1, 15140) == 0)
        #expect(vnneko_debug_variable(e, 1, 15141) == 1)
        #expect(vnneko_debug_load(e, 2))
        steps(e, 40)
        #expect(status(e).contains("part screen"), "\(status(e))")
        #expect(vnneko_debug_variable(e, 1, 15141) == 0)
        // At the pick a press on スキップ's place does nothing, and Ayano's panel starts the vote she wins.
        press(e, 1139 + 50, 25 + 20)
        steps(e, 5)
        #expect(vnneko_debug_variable(e, 1, 15141) == 0)
        press(e, 636, 200)
        press(e, 1139 + 50, 25 + 20)
        steps(e, 5)
        #expect(vnneko_debug_variable(e, 1, 15140) == 1, "\(status(e))")
    }

    // A PC save at the pick has no lines of VNDegen's beside it: the table's `resume` rule on S 105 opens the pick.
    @Test func aPCSaveAtThePickOpensThePick() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let first = try start(root)
        #expect(vnneko_debug_save(first, 3))
        vnneko_stop(first)
        let sidecar = try FileManager.default.contentsOfDirectory(at: saves(root), includingPropertiesForKeys: nil)
            .first { $0.lastPathComponent.lowercased() == "save003.vnd" }
        try FileManager.default.removeItem(at: try #require(sidecar))

        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_load(e, 3))
        steps(e, 40)
        #expect(status(e).contains("part screen"), "\(status(e))")
        press(e, 636, 200)
        press(e, 1139 + 50, 25 + 20)
        steps(e, 5)
        #expect(vnneko_debug_variable(e, 1, 15140) == 1, "\(status(e))")
        #expect(vnneko_debug_variable(e, 1, 15141) == 1)
    }

    // The vote's script runs by its own frames, 60 a second, whatever the pace of the steps: Misuzu's move down, from
    // frame 10 at 0.8 pixels a frame, has her face's top at the same row 110 frames in, stepped 16 ms or 160 ms at a time.
    @Test func aMoveStartsOnItsFrameAtAnyPaceOfSteps() throws {
        func faceTop(coarse: Bool) throws -> Int? {
            let root = try game(faces: true)
            defer { try? FileManager.default.removeItem(at: root) }
            let e = try start(root)
            defer { vnneko_stop(e) }
            press(e, 150, 500)
            // 9.9 seconds to the vote's clock and 110 frames more, less the 32 ms the press took: 11,696 ms.
            if coarse {
                for _ in 0..<73 { _ = vnneko_step(e, 160) }
                _ = vnneko_step(e, 16)
            } else {
                for _ in 0..<731 { _ = vnneko_step(e, 16) }
            }
            var w: Int32 = 0, h: Int32 = 0, changed = false
            guard let px = vnneko_frame(e, &w, &h, &changed) else { return nil }
            return (96..<640).first { y in
                let at = (y * Int(w) + 200) * 4
                return px[at] < 40 && px[at + 1] > 200 && px[at + 2] < 40
            }
        }
        let fine = try #require(try faceTop(coarse: false)), coarse = try #require(try faceTop(coarse: true))
        #expect(fine == coarse)
        #expect((127...131).contains(fine), "\(fine)")
    }
}
