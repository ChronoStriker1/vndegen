import CNekoSDK
import Foundation
import Testing
import TestSupport

// A repeating named animation (command 19) is saved as the PC saves one: its text as the command gave it and the byte
// after `animated` set, which the PC needs to keep it repeating (Test Game 88 under Wine). A load makes it repeat again,
// so a save after the load holds it the same way.
@Suite struct NekoSDKRepeatSaveTests {
    private let text = "relative_pos_y,-10,0,30,liner\r\nrelative_pos_y,10,30,30,liner"

    // A 4 by 4 red 24-bit BMP.
    private func red() -> Data {
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(54 + 48)); put(0); put(54)
        put(40); put(4); put(4)
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(48); put(0); put(0); put(0); put(0)
        for _ in 0..<16 { d.append(contentsOf: [0, 0, 255]) }
        return d
    }

    private func saveFile(_ data: URL, _ slot: Int) throws -> Data {
        let folder = data.appendingPathComponent("AppData/Roaming/waffle/alchemy/save")
        let name = String(format: "save%03d.sav", slot)
        let file = try #require(try FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: nil)
            .first { $0.lastPathComponent.lowercased() == name })
        return try NekoSDKFlowFixture.unpacked(Data(contentsOf: file))
    }

    // The animated flag and the repeat byte, which end the record's fixed part, then its frames and the text's length.
    private func flags(_ save: Data) -> (animated: UInt8, repeats: UInt8)? {
        guard let at = save.range(of: Data(text.utf8)) else { return nil }
        let start = at.lowerBound
        return (save[start - 10], save[start - 9])
    }

    @Test func aRepeatingAnimationIsSavedRepeatingAndComesBackSo() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\twaffle\\alchemy\nFirstFlow\tflow\\t.txt\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("r.bmp", red())]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 15, next: 3, p: [1: 100, 2: 100, 3: 200], s: [1: "obj\\r.bmp"]),
                .init(op: 9, next: 4),
                .init(op: 19, next: 5, p: [0: 200, 1: 1], s: [1: text]),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_press(first, "start"))
        for _ in 0..<40 { _ = vnneko_step(first, 16) }
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)
        // The picture is named twice, as the PC needs to load it (Test Game 93 under Wine): its name, then the same again.
        let first1 = try saveFile(data, 1)
        let name = Data("obj\\r.bmp".utf8)
        let at = try #require(first1.range(of: name))
        #expect(first1.subdata(in: at.upperBound + 4..<at.upperBound + 4 + name.count) == name)
        let saved = try #require(flags(first1))
        #expect(saved.animated == 1)
        #expect(saved.repeats == 1)

        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(second) }
        #expect(vnneko_debug_load(second, 1))
        // Many cycles on, it still repeats.
        for _ in 0..<200 { _ = vnneko_step(second, 16) }
        #expect(vnneko_debug_save(second, 2))
        let again = try #require(flags(try saveFile(data, 2)))
        #expect(again.animated == 1)
        #expect(again.repeats == 1)
    }
}
