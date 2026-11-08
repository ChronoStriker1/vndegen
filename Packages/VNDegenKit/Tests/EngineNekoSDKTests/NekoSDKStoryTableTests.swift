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
            "scr/e.txt": waiting(100_000),
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

        let sixth = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_load(sixth, 6))
        steps(sixth, 3)
        #expect(vnneko_debug_choice_count(sixth) == 2)
        vnneko_debug_choose(sixth, 1)
        steps(sixth, 10)
        #expect(at(sixth, "scr\\e.txt"), "\(status(sixth))")
        #expect(vnneko_debug_variable(sixth, 1, 13) == 0)
        vnneko_stop(sixth)

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
