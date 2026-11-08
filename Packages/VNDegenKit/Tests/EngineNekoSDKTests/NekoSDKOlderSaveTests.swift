import CNekoSDK
import Foundation
import Testing
import TestSupport

// The older engine's saves (Test Games 83 and 87, core/save.cpp): the choice's picture 236 bytes after the flow, sprite
// records of 383 bytes with x and y as ints, and fewer sound channels. VNDegen's save of a scene in Test Game 83's
// folder holds its standing picture where the PC's saves do, and loads back.
@Suite struct NekoSDKOlderSaveTests {
    // A 24-bit BMP of one colour, `w` by `h`, as blue, green, red.
    private func bmp(_ w: Int, _ h: Int, _ bgr: [UInt8]) -> Data {
        let row = (w * 3 + 3) & ~3
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(54 + row * h)); put(0); put(54)
        put(40); put(UInt32(w)); put(UInt32(h))
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(UInt32(row * h)); put(0); put(0); put(0); put(0)
        var line = Data()
        for _ in 0..<w { line.append(contentsOf: bgr) }
        line.append(Data(repeating: 0, count: row - w * 3))
        for _ in 0..<h { d.append(line) }
        return d
    }

    private func pixel(_ e: OpaquePointer, _ x: Int, _ y: Int) -> [UInt8] {
        _ = vnneko_step(e, 16)
        var w: Int32 = 0, h: Int32 = 0, changed = false
        guard let px = vnneko_frame(e, &w, &h, &changed) else { return [] }
        let at = (y * Int(w) + x) * 4
        return [px[at], px[at + 1], px[at + 2]]
    }

    private func int32(_ data: Data, _ at: Int) -> Int32 {
        data.subdata(in: (data.startIndex + at)..<(data.startIndex + at + 4)).withUnsafeBytes { $0.loadUnaligned(as: Int32.self) }
    }

    private func game() throws -> URL {
        try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tWaffle\\FunbagFantasy2\nFirstFlow\tflow\\t.txt\n".utf8),
            "bg.pak": NekoSDKFlowFixture.archive([("g.bmp", bmp(4, 4, [0, 255, 0]))]),
            "sc.pak": NekoSDKFlowFixture.archive([("r.bmp", bmp(101, 81, [0, 0, 255]))]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 10, next: 3, s: [1: "bg\\g.bmp"]),
                // Place 3: (1280 - 101) / 2 - 400 + 5 = 194, and (720 - 81) / 2 + 7 = 326.
                .init(op: 13, next: 4, p: [0: 3, 2: 5, 3: 7], s: [1: "sc\\r.bmp"]),
                .init(op: 9, next: 5),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
    }

    private func saves(_ data: URL) -> URL { data.appendingPathComponent("AppData/Roaming/Waffle/FunbagFantasy2/save") }

    private func file(_ data: URL, _ name: String) throws -> URL {
        try #require(try FileManager.default.contentsOfDirectory(at: saves(data), includingPropertiesForKeys: nil)
            .first { $0.lastPathComponent.lowercased() == name })
    }

    @Test func aSaveInTheOlderLayoutKeepsItsPictureAndLoads() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<10 { _ = vnneko_step(first, 16) }
        #expect(pixel(first, 194, 326) == [255, 0, 0])
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)

        // The standing picture's record: slot 600 for place 3, and x and y as ints 0xD5 after its two paths.
        let save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: try file(data, "save001.sav")))
        let path = try #require(save.range(of: Data("sc\\r.bmp".utf8)))
        let at = path.lowerBound - save.startIndex
        #expect(int32(save, at - 12) == 600)
        // After the name come the second name, empty in the older engine's saves, then the fixed part.
        let fixed = at + 8 + 4
        #expect(int32(save, at + 8) == 0)
        #expect(int32(save, fixed + 0xD5) == 194)
        #expect(int32(save, fixed + 0xD9) == 326)
        // The flow's name at 0xCD, here the patch folder's, and the choice's picture, here none, 236 bytes after it.
        let flow = Int(int32(save, 0xCD))
        #expect(flow == "sys_1\\flow\\t.txt".utf8.count)
        #expect(int32(save, 0xCD + 4 + flow + 236) == 0)

        // A new run, and the load brings back the save's picture and background.
        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(second) }
        #expect(vnneko_debug_load(second, 1))
        for _ in 0..<3 { _ = vnneko_step(second, 16) }
        #expect(pixel(second, 194, 326) == [255, 0, 0])
        #expect(pixel(second, 0, 0) == [0, 255, 0])
    }

    // A PC save of a standing picture with a named animation running names command 19's text 0x125 into the record's
    // fixed part, and all after it moves by the text's length (Test Game 87, set 1). Such a save loads.
    @Test func aRecordWithAnAnimationsTextLoads() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<10 { _ = vnneko_step(first, 16) }
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)

        var save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: try file(data, "save001.sav")))
        let path = try #require(save.range(of: Data("sc\\r.bmp".utf8)))
        let text = Data("relative_pos_y,0,0,30,liner".utf8)
        let at = path.lowerBound - save.startIndex + 8 + 4 + 0x125
        #expect(int32(save, at) == 0)
        save.replaceSubrange((save.startIndex + at)..<(save.startIndex + at + 4), with: withUnsafeBytes(of: Int32(text.count).littleEndian, { Data($0) }))
        save.insert(contentsOf: text, at: save.startIndex + at + 4)
        try NekoSDKFlowFixture.packed(save).write(to: saves(data).appendingPathComponent("save002.sav"))

        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(second) }
        #expect(vnneko_debug_load(second, 2))
        for _ in 0..<3 { _ = vnneko_step(second, 16) }
        #expect(pixel(second, 194, 326) == [255, 0, 0])
        #expect(pixel(second, 0, 0) == [0, 255, 0])
    }
}
