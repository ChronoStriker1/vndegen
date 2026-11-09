import CNekoSDK
import Foundation
import Testing
import TestSupport

// A game with no flow scripts plays the order its table's `screen story` gives (core/tables/storytest.inc): scenes, a
// game part's screens and their results, branches on them and on variables, and a part's chapter. A save names no flow,
// as the PC's saves of those games don't. VNDegen's lines beside its save keep the exact place, the last result and a
// screen open; a PC save, without them, goes on from its scene, inside the chapter too.
@Suite struct NekoSDKStoryTableTests {
    private func game() throws -> URL {
        let scene = NekoSDKFlowFixture.flow([.init(op: 0, next: 0)])
        // A scene that waits twice, so the test can find the story there and save inside it.
        func waiting(_ ms: Int32) -> Data {
            NekoSDKFlowFixture.flow([.init(op: 0, next: 2), .init(op: 2, next: 3, p: [0: ms]), .init(op: 2, next: 0, p: [0: ms])])
        }
        return try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\storytest\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("n001_000.bmp", choice())]),
            "scr/a.txt": scene,
            "scr/c2.txt": waiting(1000),
            "scr/b.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 5, next: 3, s: [2: "one"]),
                .init(op: 5, next: 0, s: [2: "two"]),
            ]),
            "scr/e.txt": waiting(1000),
        ])
    }

    // A choice's sheet, a 24-bit BMP of two rows of options.
    private func choice() -> Data {
        let w = 20, h = 10, row = (w * 3 + 3) & ~3
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(54 + row * h)); put(0); put(54)
        put(40); put(UInt32(w)); put(UInt32(h))
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(UInt32(row * h)); put(0); put(0); put(0); put(0)
        d.append(Data(repeating: 200, count: row * h))
        return d
    }

    private func status(_ e: OpaquePointer) -> String { String(cString: vnneko_debug_status(e)) }

    // The scene playing, by the table's name for it or a save's, which here is the patch folder's.
    private func at(_ e: OpaquePointer, _ scene: String) -> Bool {
        let name = status(e).components(separatedBy: " ").first ?? ""
        return name == scene || name == "sys_1\\" + scene
    }

    private func click(_ e: OpaquePointer) {
        // The middle of the frame: the input's coordinates run 0 to 1 over it.
        vnneko_mouse_move(e, 0.5, 0.5)
        vnneko_mouse_button(e, 0, true)
        vnneko_mouse_button(e, 0, false)
    }

    private func steps(_ e: OpaquePointer, _ count: Int) {
        for _ in 0..<count { _ = vnneko_step(e, 16) }
    }

    // A jump to a scene of a story table's game goes on after the scene as the table has it, as nekorun's --jump needs.
    @Test func aJumpGoesOnFromTheTable() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_jump(e, "scr\\b.txt", 3))
        // A save there holds the line's PC number, @20, as one after running to it does (set 2).
        steps(e, 3)
        #expect(vnneko_debug_save(e, 9))
        let saves = root.appendingPathComponent("data/AppData/Roaming/vndegen/storytest/save")
        let file = try #require(try FileManager.default.contentsOfDirectory(at: saves, includingPropertiesForKeys: nil)
            .first { $0.lastPathComponent.lowercased() == "save009.sav" })
        let plain = try NekoSDKFlowFixture.unpacked(Data(contentsOf: file))
        func int32(_ at: Int) -> Int32 { plain.subdata(in: (plain.startIndex + at)..<(plain.startIndex + at + 4)).withUnsafeBytes { $0.loadUnaligned(as: Int32.self) } }
        let picture = Int(int32(0xCD + 4 + 4196))
        #expect(int32(0xCD + 4 + 4196 + 4 + picture + 39596 + 404) == 20)
        // The scene's last line, then past it.
        steps(e, 200)
        click(e)
        steps(e, 200)
        #expect(vnneko_debug_choice_count(e) == 2, "\(status(e))")
    }

    @Test func theTablePlaysAndSavesKeepItsPlace() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let saves = data.appendingPathComponent("AppData/Roaming/vndegen/storytest/save")

        // A screen open before any scene: a save there names no scene, and its screen comes back.
        let opening = try #require(NekoSDKFlowFixture.start(root, data: data))
        steps(opening, 3)
        #expect(status(opening).contains("part screen"), "\(status(opening))")
        #expect(vnneko_debug_save(opening, 3))
        vnneko_stop(opening)

        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_load(first, 3))
        steps(first, 3)
        #expect(status(first).contains("part screen"), "\(status(first))")
        click(first)
        steps(first, 10)
        #expect(at(first, "scr\\c2.txt"), "\(status(first))")
        #expect(vnneko_debug_variable(first, 1, 5) == 3)
        #expect(vnneko_debug_variable(first, 1, 6) == 0)
        #expect(vnneko_debug_variable(first, 1, 7) == 0)
        #expect(vnneko_debug_variable(first, 1, 8) == 3)
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)

        // VNDegen's save: the place, and the screen's result for the `on` past the chapter.
        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_load(second, 1))
        steps(second, 3)
        #expect(at(second, "scr\\c2.txt"), "\(status(second))")
        // A load waits for a click, as the PC's does at the save's line.
        #expect(vnneko_debug_waiting_for_click(second))
        click(second)
        steps(second, 200)
        #expect(status(second).contains("part screen"), "\(status(second))")
        #expect(vnneko_debug_variable(second, 1, 11) == 0)
        // One press of two, then a save with the screen open.
        click(second)
        steps(second, 3)
        #expect(status(second).contains("part screen"), "\(status(second))")
        #expect(vnneko_debug_save(second, 2))
        vnneko_stop(second)

        // The screen comes back open with its one press, and the second finishes it.
        let third = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_load(third, 2))
        steps(third, 3)
        #expect(status(third).contains("part screen"), "\(status(third))")
        click(third)
        steps(third, 10)
        #expect(at(third, "scr\\b.txt"), "\(status(third))")
        #expect(vnneko_debug_variable(third, 1, 9) == 2)
        #expect(vnneko_debug_variable(third, 1, 10) == 0)
        // A save on the first of the two lines before the choice.
        #expect(vnneko_debug_choice_count(third) == -1)
        #expect(vnneko_debug_save(third, 5))
        vnneko_stop(third)

        // It loads on that line, waiting for a click, with the choice still to come after the second.
        let fifth = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_load(fifth, 5))
        steps(fifth, 3)
        #expect(at(fifth, "scr\\b.txt"), "\(status(fifth))")
        #expect(vnneko_debug_waiting_for_click(fifth))
        #expect(vnneko_debug_choice_count(fifth) == -1)
        let shown = vnneko_debug_text_count(fifth)
        click(fifth)
        steps(fifth, 200)
        #expect(vnneko_debug_text_count(fifth) == shown + 1)
        #expect(vnneko_debug_choice_count(fifth) == -1)
        click(fifth)
        steps(fifth, 200)
        #expect(vnneko_debug_choice_count(fifth) == 2)
        // A save while the choice waits asks it again.
        #expect(vnneko_debug_save(fifth, 6))
        vnneko_stop(fifth)

        // The save holds what the PC's do at a choice: the choice's number as the flow node, 3 for a choice waiting, and 2 for
        // the last choice's result while one waits.
        let atChoice = try #require(try FileManager.default.contentsOfDirectory(at: saves, includingPropertiesForKeys: nil)
            .first { $0.lastPathComponent.lowercased() == "save006.sav" })
        let plain = try NekoSDKFlowFixture.unpacked(Data(contentsOf: atChoice))
        func int32(_ at: Int) -> Int32 { plain.subdata(in: (plain.startIndex + at)..<(plain.startIndex + at + 4)).withUnsafeBytes { $0.loadUnaligned(as: Int32.self) } }
        let picture = Int(int32(0xCD + 4 + 4196))
        let date = 0xCD + 4 + 4196 + 4 + picture + 39596
        #expect(int32(date + 400) == 3)
        #expect(int32(date + 404) == 25)
        #expect(int32(date + 520) == 2)
        // As a PC save, without VNDegen's lines beside it, it goes on from the line with its number: the choice, asked again.
        try FileManager.default.copyItem(at: atChoice, to: saves.appendingPathComponent("save007.sav"))
        let seventh = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_load(seventh, 7))
        steps(seventh, 3)
        #expect(vnneko_debug_choice_count(seventh) == 2)
        vnneko_stop(seventh)

        let sixth = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_load(sixth, 6))
        steps(sixth, 3)
        #expect(vnneko_debug_choice_count(sixth) == 2)
        vnneko_debug_choose(sixth, 1)
        steps(sixth, 10)
        #expect(at(sixth, "scr\\e.txt"), "\(status(sixth))")
        #expect(vnneko_debug_variable(sixth, 1, 13) == 0)
        // A save in the scene after the choice holds 2 where the last choice goes, with `lastchoice made`, whichever
        // option was taken.
        #expect(vnneko_debug_save(sixth, 8))
        let after = try #require(try FileManager.default.contentsOfDirectory(at: saves, includingPropertiesForKeys: nil)
            .first { $0.lastPathComponent.lowercased() == "save008.sav" })
        let afterPlain = try NekoSDKFlowFixture.unpacked(Data(contentsOf: after))
        func int32After(_ at: Int) -> Int32 { afterPlain.subdata(in: (afterPlain.startIndex + at)..<(afterPlain.startIndex + at + 4)).withUnsafeBytes { $0.loadUnaligned(as: Int32.self) } }
        let afterPicture = Int(int32After(0xCD + 4 + 4196))
        let afterDate = 0xCD + 4 + 4196 + 4 + afterPicture + 39596
        #expect(int32After(afterDate + 400) == 1)
        #expect(int32After(afterDate + 404) == 30)
        #expect(int32After(afterDate + 520) == 2)
        vnneko_stop(sixth)

        // As a PC save, its number is also the first scene's, and its scene picks the last: the story ends after it rather
        // than going on to the screen after the first.
        try FileManager.default.copyItem(at: after, to: saves.appendingPathComponent("save009.sav"))
        let ninth = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_load(ninth, 9))
        steps(ninth, 3)
        #expect(at(ninth, "scr\\e.txt"), "\(status(ninth))")
        click(ninth)
        steps(ninth, 200)
        click(ninth)
        steps(ninth, 200)
        #expect(!status(ninth).contains("part screen"), "\(status(ninth))")
        vnneko_stop(ninth)

        // A PC save has no lines beside it: the story goes on from its scene, here inside the chapter.
        let sidecar = try FileManager.default.contentsOfDirectory(at: saves, includingPropertiesForKeys: nil)
            .first { $0.lastPathComponent.lowercased() == "save001.vnd" }
        try FileManager.default.removeItem(at: try #require(sidecar))
        let fourth = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(fourth) }
        #expect(vnneko_debug_load(fourth, 1))
        steps(fourth, 3)
        #expect(at(fourth, "scr\\c2.txt"), "\(status(fourth))")
        #expect(vnneko_debug_variable(fourth, 1, 8) == 3)
        click(fourth)
        steps(fourth, 200)
        #expect(status(fourth).contains("part screen"), "\(status(fourth))")
    }
}
