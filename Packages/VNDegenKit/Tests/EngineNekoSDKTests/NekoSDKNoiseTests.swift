import CNekoSDK
import Foundation
import Testing
import TestSupport

// The SDK's film noise: command 51 puts a crop of its frame over the scene, in slot 10000, which a save keeps as the PC's
// saves do, a record of kind 4 with no picture, and a load brings back.
@Suite struct NekoSDKNoiseTests {
    // A 2 by 2 red picture as a 24-bit BMP, standing in for the noise frame: its crop is the top left pixel.
    private func red() -> Data {
        var data = Data()
        func put<T: FixedWidthInteger>(_ v: T) { data.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        let rows = 8 * 2
        data.append(contentsOf: [0x42, 0x4D])
        put(UInt32(54 + rows))
        put(UInt32(0))
        put(UInt32(54))
        put(UInt32(40))
        put(Int32(2))
        put(Int32(2))
        put(UInt16(1))
        put(UInt16(24))
        for _ in 0..<6 { put(UInt32(0)) }
        for _ in 0..<2 { data.append(contentsOf: [0, 0, 255, 0, 0, 255, 0, 0]) }
        return data
    }

    private func game() throws -> URL {
        try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\twaffle\\alchemy\nFirstFlow\tflow\\t.txt\n".utf8),
            "sys/noise2_small.bmp": red(),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 51, next: 3),
                .init(op: 9, next: 4),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
    }

    private func topLeft(_ e: OpaquePointer) -> [UInt8] {
        _ = vnneko_step(e, 16)
        var w: Int32 = 0, h: Int32 = 0, changed: Bool = false
        guard let px = vnneko_frame(e, &w, &h, &changed) else { return [] }
        return [px[0], px[1], px[2]]
    }

    @Test func noiseDrawsAndASaveKeepsIt() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<10 { _ = vnneko_step(first, 16) }
        #expect(topLeft(first) == [255, 0, 0])
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)

        // The record the PC writes for it: not an ordinary picture, slot 10000, kind 4.
        let save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: data.appendingPathComponent("AppData/Roaming/waffle/alchemy/save/save001.sav")))
        #expect(save.range(of: Data([1, 0, 0x10, 0x27, 0, 0, 4, 0, 0, 0])) != nil)

        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(second) }
        #expect(topLeft(second) != [255, 0, 0])
        #expect(vnneko_debug_load(second, 1))
        for _ in 0..<3 { _ = vnneko_step(second, 16) }
        #expect(topLeft(second) == [255, 0, 0])
    }
}
