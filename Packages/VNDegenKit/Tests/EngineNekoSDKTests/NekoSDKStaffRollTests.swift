import CNekoSDK
import Foundation
import Testing
import TestSupport

// Funbag Fantasy 3 if's staff roll, which its exe draws: 107.7 seconds of its table's layers, after which the flow goes
// on and GLB_99_ENDING (1050) is set. A click ends it only once that was set before it began, as under Wine.
@Suite struct NekoSDKStaffRollTests {
    private let macros = "#p Global\nGLB_00_PROLOGUE_OVER\t1000\nGLB_01_OVER\t1051\nGLB_99_ENDING\t1050\n##\n"

    private func run(seen: Bool, click: Bool, _ check: (OpaquePointer, Int) -> Void) throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\twaffle\\FunbagFantasy3\n".utf8),
            "cfg/macrolist.ini": Data(macros.utf8),
            // The staff roll, then the chapter's flag (place 1 by name, 1051), then the end.
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 2, next: 3, p: [0: 1]),
                .init(op: 6, next: 4, p: [0: 0, 1: 1, 4: 2, 5: 1]),
                .init(op: 2, next: 0),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        if seen {
            let save = data.appendingPathComponent("AppData/Roaming/waffle/FunbagFantasy3/save/cfg.sav")
            try FileManager.default.createDirectory(at: save.deletingLastPathComponent(), withIntermediateDirectories: true)
            var plain = Data(count: 104_000)
            plain[4000 + 1050 * 4] = 1
            try NekoSDKFlowFixture.packed(plain).write(to: save)
        }
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_jump(e, "flow\\t.txt", 0))
        // 30 seconds in, the flag isn't set yet and the roll goes on.
        for _ in 0..<300 { _ = vnneko_step(e, 100) }
        #expect(vnneko_debug_variable(e, 0, 1051) == 0)
        if click {
            vnneko_mouse_button(e, 0, true)
            vnneko_mouse_button(e, 0, false)
            for _ in 0..<5 { _ = vnneko_step(e, 100) }
            check(e, 30)
            return
        }
        for _ in 0..<800 { _ = vnneko_step(e, 100) }
        check(e, 110)
    }

    @Test func itRunsItsLengthThenGoesOnAndMarksItSeen() throws {
        try run(seen: false, click: false) { e, _ in
            #expect(vnneko_debug_variable(e, 0, 1051) == 1)
            #expect(vnneko_debug_variable(e, 0, 1050) == 1)
        }
    }

    @Test func aClickDoesNothingTheFirstTime() throws {
        try run(seen: false, click: true) { e, _ in
            #expect(vnneko_debug_variable(e, 0, 1051) == 0)
        }
    }

    @Test func aClickEndsItOnceSeen() throws {
        try run(seen: true, click: true) { e, _ in
            #expect(vnneko_debug_variable(e, 0, 1051) == 1)
        }
    }
}
