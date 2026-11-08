import CNekoSDK
import Foundation
import Testing
import TestSupport

// What the PC keeps for its save cards in cfg.sav, as seen under Wine in The Alchemist, Yarinaoshi no Christmas and Test
// Games 92 and 94: the card saved to last, which shows NEW, in global 0, and a card's lock in global 199 plus its number.
@Suite struct NekoSDKSaveCardTests {
    private func game() throws -> URL {
        try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\twaffle\\alchemy\n".utf8),
        ])
    }

    @Test func newAndLocksAreGlobalsInCfgSav() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let cfg = data.appendingPathComponent("AppData/Roaming/waffle/alchemy/save/cfg.sav")
        func global(_ number: Int) throws -> UInt8 { try NekoSDKFlowFixture.unpacked(Data(contentsOf: cfg))[4000 + number * 4] }

        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_save(first, 3))
        #expect(vnneko_debug_variable(first, 0, 0) == 3)
        #expect(try global(0) == 3)
        vnneko_debug_toggle_lock(first, 3)
        #expect(vnneko_debug_variable(first, 0, 202) == 1)
        #expect(try global(202) == 1)
        #expect(try global(200) == 0)
        vnneko_stop(first)

        // A new session reads both back, and the lock turns off again.
        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(second) }
        #expect(vnneko_debug_variable(second, 0, 0) == 3)
        #expect(vnneko_debug_variable(second, 0, 202) == 1)
        vnneko_debug_toggle_lock(second, 3)
        #expect(vnneko_debug_variable(second, 0, 202) == 0)
        #expect(try global(202) == 0)
    }

    // A card's comment, typed on the PC, is the string after the save's first byte, and everything after it moves along.
    // VNDegen 0.26.2 read a fixed place there, so such a save from a PC showed as an empty card and didn't load.
    @Test func aSaveWithAPCCommentLoads() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let file = data.appendingPathComponent("AppData/Roaming/waffle/alchemy/save/save001.sav")

        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)
        var plain = try NekoSDKFlowFixture.unpacked(Data(contentsOf: file))
        #expect(plain[0] == 1 && plain[1..<5].allSatisfy { $0 == 0 })
        let comment = try #require("あいう".data(using: .shiftJIS))
        plain.replaceSubrange(1..<5, with: withUnsafeBytes(of: UInt32(comment.count).littleEndian, Array.init) + comment)
        try NekoSDKFlowFixture.packed(plain).write(to: file)

        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(second) }
        #expect(vnneko_debug_load(second, 1))
    }
}
