import CNekoSDK
import Foundation
import Testing
import TestSupport

// A game with no flow scripts plays the order its table's `screen story` gives (core/tables/storytest.inc): scenes, a
// game part's screen and its result, branches on it and on variables, and a part's chapter. A save names no flow, as the
// PC's saves of those games don't, and a load goes on from the save's scene, inside the chapter too.
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

    // The scene playing, by the table's name for it or a save's, which here is the patch folder's.
    private func at(_ e: OpaquePointer, _ scene: String) -> Bool {
        let name = String(cString: vnneko_debug_status(e)).components(separatedBy: " ").first ?? ""
        return name == scene || name == "sys_1\\" + scene
    }

    @Test func theTablePlaysAndASaveGoesOnFromItsScene() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<10 { _ = vnneko_step(first, 16) }
        #expect(at(first, "scr\\c2.txt"), "\(String(cString: vnneko_debug_status(first)))")
        #expect(vnneko_debug_variable(first, 1, 5) == 3)
        #expect(vnneko_debug_variable(first, 1, 6) == 0)
        #expect(vnneko_debug_variable(first, 1, 7) == 0)
        #expect(vnneko_debug_variable(first, 1, 8) == 3)
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)

        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(second) }
        #expect(vnneko_debug_load(second, 1))
        for _ in 0..<3 { _ = vnneko_step(second, 16) }
        #expect(at(second, "scr\\c2.txt"), "\(String(cString: vnneko_debug_status(second)))")
        #expect(vnneko_debug_variable(second, 1, 8) == 3)
        // A load waits for a click, as the PC's does at the save's line. Then the chapter's scene ends, and the flow goes on
        // after the chapter.
        #expect(vnneko_debug_waiting_for_click(second))
        vnneko_mouse_move(second, 640, 360)
        vnneko_mouse_button(second, 0, true)
        vnneko_mouse_button(second, 0, false)
        for _ in 0..<200 { _ = vnneko_step(second, 16) }
        #expect(at(second, "scr\\b.txt"), "\(String(cString: vnneko_debug_status(second)))")
    }
}
