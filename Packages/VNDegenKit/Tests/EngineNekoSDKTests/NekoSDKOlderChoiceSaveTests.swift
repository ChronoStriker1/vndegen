import CNekoSDK
import Foundation
import Testing
import TestSupport

// Test Game 87's saves name a choice's picture in the older engine's first place, 236 bytes after the flow, with the
// newer place empty, and hold 1 360 bytes into the date block only once a choice has been made, as its PC writes them
// (set 1, under Wine): its PC shows a choice with no options from a save with the picture in the newer place. Its cfg.sav
// counts the NEW card from 0.
@Suite struct NekoSDKOlderChoiceSaveTests {
    // A 24-bit BMP of two rows of options.
    private func sheet() -> Data {
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

    private struct Fields { var first: String, newer: String, flag: Int32 }

    // The two picture names and the flag, from a save's start: the flow's name at 0xCD, the first place 236 bytes after it,
    // the newer place 4,196 bytes after it less the first name's length, and the date block 39,596 bytes after that.
    private func fields(_ data: URL, _ slot: Int) throws -> Fields {
        let folder = data.appendingPathComponent("AppData/Roaming/waffle/funbag_fantasy_sideboob_story_2/save")
        let name = String(format: "save%03d.sav", slot)
        let file = try #require(try FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: nil)
            .first { $0.lastPathComponent.lowercased() == name })
        let save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: file))
        func int32(_ at: Int) -> Int32 {
            save.subdata(in: (save.startIndex + at)..<(save.startIndex + at + 4)).withUnsafeBytes { $0.loadUnaligned(as: Int32.self) }
        }
        func string(_ at: Int) -> String {
            let n = Int(int32(at))
            return String(decoding: save.subdata(in: (save.startIndex + at + 4)..<(save.startIndex + at + 4 + n)), as: UTF8.self)
        }
        let afterFlow = 0xCD + 4 + Int(int32(0xCD))
        let first = string(afterFlow + 236)
        let newerAt = afterFlow + 236 + 4 + first.utf8.count + 4196 - 236 - 4
        let newer = string(newerAt)
        let date = newerAt + 4 + newer.utf8.count + 39596
        return Fields(first: first, newer: newer, flag: int32(date + 360))
    }

    @Test func aChoiceIsSavedWhere87sPCReadsIt() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\twaffle\\funbag_fantasy_sideboob_story_2\nFirstFlow\tflow\\t.txt\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("l001_000.bmp", sheet()), ("l001_001.bmp", sheet())]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 3, next: 3, p: [0: 1, 1: 1], s: [1: "obj\\l001_000.bmp"]),
                .init(op: 3, next: 4, p: [0: 1, 1: 1], s: [1: "obj\\l001_001.bmp"]),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<5 { _ = vnneko_step(first, 16) }
        // With 87's table its title comes first, and Start runs the first flow (set 1); without one the flow is already
        // running, and there's no Start to press.
        _ = vnneko_debug_press(first, "start")
        for _ in 0..<5 { _ = vnneko_step(first, 16) }
        #expect(vnneko_debug_choice_count(first) == 2)
        // At the first choice, none made yet.
        #expect(vnneko_debug_save(first, 1))
        let atFirst = try fields(data, 1)
        #expect(atFirst.first == "obj\\l001_000.bmp")
        #expect(atFirst.newer.isEmpty)
        #expect(atFirst.flag == 0)
        vnneko_debug_choose(first, 0)
        for _ in 0..<5 { _ = vnneko_step(first, 16) }
        #expect(vnneko_debug_choice_count(first) == 2)
        // At the second, one made.
        #expect(vnneko_debug_save(first, 2))
        let atSecond = try fields(data, 2)
        #expect(atSecond.first == "obj\\l001_001.bmp")
        #expect(atSecond.newer.isEmpty)
        #expect(atSecond.flag == 1)
        vnneko_stop(first)
        // Global 0 names the card NEW marks counting from 0 in this game, so 1 for the second card: one past the last card
        // crashes its PC's Load page.
        let cfg = try NekoSDKFlowFixture.unpacked(Data(contentsOf: try #require(try FileManager.default.contentsOfDirectory(
            at: data.appendingPathComponent("AppData/Roaming/waffle/funbag_fantasy_sideboob_story_2/save"), includingPropertiesForKeys: nil)
            .first { $0.lastPathComponent.lowercased() == "cfg.sav" })))
        #expect(cfg.subdata(in: (cfg.startIndex + 4000)..<(cfg.startIndex + 4004)).withUnsafeBytes { $0.loadUnaligned(as: Int32.self) } == 1)

        // Each loads and saves again as it was.
        for (slot, again, flag) in [(1, 3, Int32(0)), (2, 4, Int32(1))] {
            let e = try #require(NekoSDKFlowFixture.start(root, data: data))
            #expect(vnneko_debug_load(e, Int32(slot)))
            for _ in 0..<5 { _ = vnneko_step(e, 16) }
            #expect(vnneko_debug_save(e, Int32(again)))
            vnneko_stop(e)
            let resaved = try fields(data, again)
            #expect(resaved.first == (slot == 1 ? "obj\\l001_000.bmp" : "obj\\l001_001.bmp"))
            #expect(resaved.newer.isEmpty)
            #expect(resaved.flag == flag)
        }
    }
}
