import CNekoSDK
import Foundation
import Testing
import TestSupport

// Test Game 94's route mode and card locks, kept where its PC keeps them, as seen under Wine: the route in the scenario
// variable SENARIO_STATUS (4000), which its saves carry, and a card's lock in global 199 plus the card's number.
@Suite struct NekoSDKRinjinSaveTests {
    private let folder = "waffle\\隣人に壊されていく俺の妻"

    // Two choices that route 1 answers with the lower option, each followed by a branch that records the answer in
    // SENARIO_ANSWER1 and SENARIO_ANSWER2: 1 for the lower option, 2 for the upper.
    private func game() throws -> URL {
        try NekoSDKFlowFixture.game([
            "cfg/init.ini": try #require("UserDataDirectory\t\(folder)\nFirstFlow\tflow\\t.txt\n".data(using: .shiftJIS)),
            "cfg/macrolist.ini": Data("#p Global\nGLB_ENDING_A\t51\n##\n#p Senario\nSENARIO_ANSWER1\t7\nSENARIO_ANSWER2\t8\nSENARIO_STATUS\t4000\n##\n".utf8),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 3, next: 3, p: [0: 1, 1: 1, 10: 1], s: [1: "obj\\n001_000.bmp"]),
                .init(op: 4, next: 5, otherwise: 4, p: [0: 1]),
                .init(op: 6, next: 6, p: [0: 1, 2: 0, 4: 2, 5: 1]),
                .init(op: 6, next: 6, p: [0: 1, 2: 0, 4: 2, 5: 2]),
                .init(op: 3, next: 7, p: [0: 1, 1: 1, 10: 1], s: [1: "obj\\n001_000.bmp"]),
                .init(op: 4, next: 9, otherwise: 8, p: [0: 1]),
                .init(op: 6, next: 10, p: [0: 1, 2: 1, 4: 2, 5: 1]),
                .init(op: 6, next: 10, p: [0: 1, 2: 1, 4: 2, 5: 2]),
                .init(op: 2, next: 0),
            ]),
        ])
    }

    @Test func aRouteIsSENARIO_STATUSAndASaveCarriesIt() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")

        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_press(first, "route1"))
        #expect(vnneko_debug_variable(first, 1, 4000) == 1)
        #expect(vnneko_debug_save(first, 3))
        for _ in 0..<5 { _ = vnneko_step(first, 16) }
        #expect(vnneko_debug_variable(first, 1, 7) == 1)
        #expect(vnneko_debug_variable(first, 1, 8) == 1)
        vnneko_stop(first)

        // A new start isn't in a route, and the second choice isn't answered for it.
        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(second) }
        #expect(vnneko_debug_jump(second, "flow\\t.txt", 6))
        for _ in 0..<5 { _ = vnneko_step(second, 16) }
        #expect(vnneko_debug_variable(second, 1, 8) != 1)

        // Loading the save made in route 1 goes on in it.
        #expect(vnneko_debug_load(second, 3))
        #expect(vnneko_debug_variable(second, 1, 4000) == 1)
        #expect(vnneko_debug_jump(second, "flow\\t.txt", 6))
        for _ in 0..<5 { _ = vnneko_step(second, 16) }
        #expect(vnneko_debug_variable(second, 1, 8) == 1)
    }

    @Test func aCardsLockIsAGlobalInCfgSav() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let cfg = data.appendingPathComponent("AppData/Roaming/\(folder.replacingOccurrences(of: "\\", with: "/"))/save/cfg.sav")
        func lockByte(_ slot: Int) throws -> UInt8 { try NekoSDKFlowFixture.unpacked(Data(contentsOf: cfg))[4000 + (199 + slot) * 4] }

        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_press(e, "route1"))
        #expect(vnneko_debug_save(e, 5))
        vnneko_debug_toggle_lock(e, 5)
        #expect(vnneko_debug_variable(e, 0, 204) == 1)
        #expect(try lockByte(5) == 1)
        #expect(try lockByte(1) == 0)
        vnneko_debug_toggle_lock(e, 5)
        #expect(vnneko_debug_variable(e, 0, 204) == 0)
        #expect(try lockByte(5) == 0)
        vnneko_stop(e)
    }
}
