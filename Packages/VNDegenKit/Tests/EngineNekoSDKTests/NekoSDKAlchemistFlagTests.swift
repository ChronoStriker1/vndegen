import CNekoSDK
import Foundation
import Testing
import TestSupport

// The Alchemist's flags, as its macrolist.ini and flows give them (#58, #59).
@Suite struct NekoSDKAlchemistFlagTests {
    private let macros = """
    #p Global
    GLB_PROLOGUE_OVER\t1000\t//プロローグを見た？
    GLB_ENDING\t\t1050\t//ENDINGまで行った？
    ##
    #p Senario
    SENARIO_110_HAPPY\t\t110\t//ヒロイン選択回数:HAPPYEND
    SENARIO_STATUS\t\t4000\t//ルートステータス
    ##
    """

    // flow06's ending: `HAPPYEND_COUNTER <= 3 の時` holding takes its other target, 正規ルートEND, and the happy ending is
    // `next`, for four happy choices or more (#58). Here the endings mark SENARIO_STATUS 2 and 1.
    private func ending(happyChoices: Int32) throws -> Int {
        var status = 0
        try NekoSDKFlowFixture.run([
            .init(op: 0, next: 2),
            .init(op: 6, next: 3, p: [0: 1, 4: 2, 5: happyChoices]),
            .init(op: 5, next: 4, otherwise: 5, p: [0: 1, 3: 2, 4: 2, 5: 3]),
            .init(op: 6, next: 6, p: [0: 1, 1: 1, 2: 1, 4: 2, 5: 1]),
            .init(op: 6, next: 6, p: [0: 1, 1: 1, 2: 1, 4: 2, 5: 2]),
            .init(op: 2, next: 0),
        ], macros: macros) { e in
            #expect(vnneko_debug_variable(e, 1, 110) == Int(happyChoices))
            status = Int(vnneko_debug_variable(e, 1, 4000))
        }
        return status
    }

    @Test func theEndingFollowsTheHappyCount() throws {
        #expect(try ending(happyChoices: 3) == 2)
        #expect(try ending(happyChoices: 4) == 1)
        #expect(try ending(happyChoices: 7) == 1)
    }

    // The prologue's end names place 1, which by name is GLB_PROLOGUE_OVER, not GLB_ENDING (#59).
    @Test func placesCountInNameOrder() throws {
        let root = try NekoSDKFlowFixture.game(["cfg/macrolist.ini": Data(macros.utf8)])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(vnneko_debug_macro_number(root.path, 0, 0) == 1050)
        #expect(vnneko_debug_macro_number(root.path, 0, 1) == 1000)
        #expect(vnneko_debug_macro_number(root.path, 1, 0) == 110)
        #expect(vnneko_debug_macro_number(root.path, 1, 1) == 4000)
    }

    // A cfg.sav VNDegen 0.26.1 wrote after the prologue has GLB_ENDING and not GLB_PROLOGUE_OVER. Reading it sets the
    // prologue's flag and writes the file back, once.
    @Test func a0261FileGetsItsPrologueFlag() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\twaffle\\alchemy\n".utf8),
            "cfg/macrolist.ini": Data(macros.utf8),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let save = data.appendingPathComponent("AppData/Roaming/waffle/alchemy/save/cfg.sav")
        try FileManager.default.createDirectory(at: save.deletingLastPathComponent(), withIntermediateDirectories: true)
        var plain = Data(count: 104_000)
        plain[4000 + 1050 * 4] = 1
        try NekoSDKFlowFixture.packed(plain).write(to: save)

        func global(_ file: Data, _ number: Int) -> UInt8 { file[4000 + number * 4] }
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_variable(first, 0, 1000) == 1)
        #expect(vnneko_debug_variable(first, 0, 1050) == 1)
        vnneko_stop(first)
        let repaired = try Data(contentsOf: save)
        let repairedPlain = try NekoSDKFlowFixture.unpacked(repaired)
        #expect(global(repairedPlain, 1000) == 1)
        #expect(global(repairedPlain, 1050) == 1)

        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_variable(second, 0, 1000) == 1)
        vnneko_stop(second)
        #expect(try Data(contentsOf: save) == repaired)
    }

    // A file with neither flag, or the PC's own after the prologue, is left as it is.
    @Test func otherFilesAreLeftAlone() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\twaffle\\alchemy\n".utf8),
            "cfg/macrolist.ini": Data(macros.utf8),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let save = data.appendingPathComponent("AppData/Roaming/waffle/alchemy/save/cfg.sav")
        try FileManager.default.createDirectory(at: save.deletingLastPathComponent(), withIntermediateDirectories: true)
        var plain = Data(count: 104_000)
        plain[4000 + 1000 * 4] = 1
        let file = NekoSDKFlowFixture.packed(plain)
        try file.write(to: save)
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_variable(e, 0, 1000) == 1)
        #expect(vnneko_debug_variable(e, 0, 1050) == 0)
        vnneko_stop(e)
        #expect(try Data(contentsOf: save) == file)
    }
}
