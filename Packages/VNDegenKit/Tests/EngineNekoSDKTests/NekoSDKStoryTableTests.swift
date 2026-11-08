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
            "scr/a.txt": scene,
            "scr/c2.txt": waiting(1000),
            "scr/b.txt": waiting(100_000),
        ])
    }

    private func status(_ e: OpaquePointer) -> String { String(cString: vnneko_debug_status(e)) }

    // The scene playing, by the table's name for it or a save's, which here is the patch folder's.
    private func at(_ e: OpaquePointer, _ scene: String) -> Bool {
        let name = status(e).components(separatedBy: " ").first ?? ""
        return name == scene || name == "sys_1\\" + scene
    }

    private func click(_ e: OpaquePointer) {
        vnneko_mouse_move(e, 640, 360)
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

        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
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
        vnneko_stop(third)

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
