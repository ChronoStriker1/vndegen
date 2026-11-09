import CNekoSDK
import Foundation
import Testing
import TestSupport

// A game part's picture through PartHost::loadImage (the selftest part's command 9000): placed by its top left, tiled
// along the axis it scrolls on and moving at its speed, as Test Game 86's lace strips do, or adding its light, as Test
// Game 83's streaks do; and a save keeps both.
@Suite struct NekoSDKPartImageTests {
    // A 24-bit BMP, `w` by `h`, each pixel's colour as red, green, blue from `colour(x)`.
    private func bmp(_ w: Int, _ h: Int, _ colour: (Int) -> [UInt8]) -> Data {
        let row = (w * 3 + 3) & ~3
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(54 + row * h)); put(0); put(54)
        put(40); put(UInt32(w)); put(UInt32(h))
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(UInt32(row * h)); put(0); put(0); put(0); put(0)
        var line = Data()
        for x in 0..<w { let c = colour(x); line.append(contentsOf: [c[2], c[1], c[0]]) }
        line.append(Data(repeating: 0, count: row - w * 3))
        for _ in 0..<h { d.append(line) }
        return d
    }

    private func game() throws -> URL {
        try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\storytest\nFirstFlow\tflow\\t.txt\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([
                // A strip with a red band 10 wide every 100.
                ("lace.bmp", bmp(100, 40) { $0 < 10 ? [255, 0, 0] : [0, 0, 255] }),
                ("red.bmp", bmp(1280, 720) { _ in [255, 0, 0] }),
                ("dim.bmp", bmp(10, 10) { _ in [0, 0, 64] }),
            ]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 15, next: 3, p: [3: 100], s: [1: "obj\\red.bmp"]),
                // The strip at y 300, scrolling 100 px a second to the left; the dim square adding its light at 600, 10.
                .init(op: 9000, next: 4, p: [1: 0, 2: 300, 3: 150, 5: -100], s: [1: "obj\\lace.bmp"]),
                .init(op: 9000, next: 5, p: [1: 600, 2: 10, 3: 200, 4: 1], s: [1: "obj\\dim.bmp"]),
                .init(op: 9, next: 6),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
    }

    private func frame(_ e: OpaquePointer) -> (w: Int, px: [UInt8]) {
        var w: Int32 = 0, h: Int32 = 0, changed = false
        guard let px = vnneko_frame(e, &w, &h, &changed) else { return (0, []) }
        return (Int(w), Array(UnsafeBufferPointer(start: px, count: Int(w) * Int(h) * 4)))
    }

    // Where the strip's first red band starts along row 320, from 0 to 99.
    private func bandStart(_ e: OpaquePointer) -> Int? {
        let f = frame(e)
        for x in 0..<200 {
            let at = (320 * f.w + x) * 4
            let prev = (320 * f.w + (x + f.w - 1) % f.w) * 4
            if f.px[at] > 200 && f.px[at + 2] < 50 && !(f.px[prev] > 200 && f.px[prev + 2] < 50) { return x % 100 }
        }
        return nil
    }

    private func steps(_ e: OpaquePointer, _ ms: Int) { for _ in 0..<(ms / 10) { _ = vnneko_step(e, 10) } }

    // Two places round the strip's 100 pixels within a pixel of each other.
    private func near(_ a: Int, _ b: Int) -> Bool { let d = abs(a - b) % 100; return min(d, 100 - d) <= 1 }

    @Test func aPartsPictureScrollsTiledAddsItsLightAndSaves() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        steps(e, 300)
        let first = try #require(bandStart(e))
        steps(e, 500)
        // Half a second at 100 px a second to the left: 50 further left, round the strip's 100.
        let second = try #require(bandStart(e))
        #expect(near(second, first + 50), "band at \(first), then \(second)")
        // Tiled: a band well past the strip's own 100 pixels too.
        let f = frame(e)
        let bands = (0..<1280).filter { x in let at = (320 * f.w + x) * 4; return f.px[at] > 200 && f.px[at + 2] < 50 }.count
        #expect(bands >= 120)
        // Added: the dim blue over red gives both.
        let dim = (15 * f.w + 605) * 4
        #expect(Array(f.px[dim..<dim + 3]) == [255, 0, 64])

        #expect(vnneko_debug_save(e, 1))
        vnneko_stop(e)
        let again = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(again) }
        #expect(vnneko_debug_load(again, 1))
        steps(again, 300)
        let loaded = try #require(bandStart(again))
        steps(again, 500)
        let after = try #require(bandStart(again))
        #expect(near(after, loaded + 50), "band at \(loaded), then \(after)")
        let g = frame(again)
        let dimAgain = (15 * g.w + 605) * 4
        #expect(Array(g.px[dimAgain..<dimAgain + 3]) == [255, 0, 64])
    }

    // A slot a part holds with no picture (PartHost::holdSlot, the selftest part's command 9001): the part draws it in
    // the slot's order once a screen update shows it, command 17 takes it away as it does a picture, and a save in the
    // older layout holds its record as Test Game 83's PC writes a particle emitter's: kind 15, no picture, three names 57
    // bytes into the fixed part and the part's own bytes in it. A load brings the slot back for the part to fill.
    @Test func aPartsOwnSlotDrawsSavesAndGoes() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\partslot\nFirstFlow\tflow\\t.txt\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("red.bmp", bmp(1280, 720) { _ in [255, 0, 0] })]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 15, next: 3, p: [3: 100], s: [1: "obj\\red.bmp"]),
                .init(op: 9001, next: 4, p: [0: 500], s: [1: "parts\\one.bmp"]),
                .init(op: 9, next: 5),
                .init(op: 2, next: 6, p: [0: 3000]),
                .init(op: 17, next: 7, p: [0: 500]),
                .init(op: 9, next: 8),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        func pixel(_ e: OpaquePointer, _ x: Int, _ y: Int) -> [UInt8] {
            let f = frame(e)
            let at = (y * f.w + x) * 4
            return [f.px[at], f.px[at + 1], f.px[at + 2]]
        }
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(e) }
        steps(e, 500)
        // The part's square, over the red picture in slot 100.
        #expect(pixel(e, 605, 305) == [0, 255, 0])
        #expect(pixel(e, 590, 305) == [255, 0, 0])
        #expect(vnneko_debug_save(e, 1))

        let saves = data.appendingPathComponent("AppData/Roaming/vndegen/partslot/save")
        let file = try #require(try FileManager.default.contentsOfDirectory(at: saves, includingPropertiesForKeys: nil)
            .first { $0.lastPathComponent.lowercased() == "save001.sav" })
        let save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: file))
        func int32(_ at: Int) -> Int32 { save.subdata(in: (save.startIndex + at)..<(save.startIndex + at + 4)).withUnsafeBytes { $0.loadUnaligned(as: Int32.self) } }
        let name = try #require(save.range(of: Data("parts\\one.bmp".utf8))).lowerBound - save.startIndex
        // The record's fixed part starts 61 before the name: its slot and kind before its two empty names, the name's
        // length 57 in, the part's 7 at 53, and the two other names empty after it.
        let fixed = name - 61
        #expect(int32(fixed - 16) == 500)
        #expect(int32(fixed - 12) == 15)
        #expect(int32(fixed - 8) == 0 && int32(fixed - 4) == 0)
        #expect(int32(fixed + 57) == 13)
        #expect(int32(fixed + 53) == 7)
        #expect(int32(name + 13) == 0 && int32(name + 17) == 0)

        // A new run: the load brings the slot back, and the part reads the picture's name from its record.
        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(second) }
        #expect(vnneko_debug_load(second, 1))
        steps(second, 100)
        #expect(pixel(second, 605, 305) == [0, 255, 0])

        // Command 17 and the next update take the slot away.
        steps(e, 3500)
        #expect(pixel(e, 605, 305) == [255, 0, 0])
    }

    // A PC save's record of a tiled, moving picture, kind 6 with its speed as a float 45 bytes into the fixed part, comes
    // back tiled and moving at 60 times that in pixels a second, with no lines of VNDegen's beside the save.
    @Test func aPCSavesTiledPictureMovesAgain() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\partslot\nFirstFlow\tflow\\t.txt\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("lace.bmp", bmp(100, 40) { $0 < 10 ? [255, 0, 0] : [0, 0, 255] })]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 9000, next: 3, p: [1: 0, 2: 300, 3: 150], s: [1: "obj\\lace.bmp"]),
                .init(op: 9, next: 4),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        steps(first, 300)
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)

        // As the PC writes it: the record's kind 6, 8 bytes before the name's length, and 0.5 at 45 past the two names.
        let saves = data.appendingPathComponent("AppData/Roaming/vndegen/partslot/save")
        let files = try FileManager.default.contentsOfDirectory(at: saves, includingPropertiesForKeys: nil)
        let file = try #require(files.first { $0.lastPathComponent.lowercased() == "save001.sav" })
        try FileManager.default.removeItem(at: try #require(files.first { $0.lastPathComponent.lowercased() == "save001.vnd" }))
        var save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: file))
        let name = try #require(save.range(of: Data("obj\\lace.bmp".utf8))).lowerBound - save.startIndex
        func put<T>(_ value: T, _ at: Int) { save.replaceSubrange((save.startIndex + at)..<(save.startIndex + at + 4), with: withUnsafeBytes(of: value, { Data($0) })) }
        put(Int32(6).littleEndian, name - 8)
        put(Float(0.5).bitPattern.littleEndian, name + 12 + 4 + 45)
        try NekoSDKFlowFixture.packed(save).write(to: file)

        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_load(e, 1))
        steps(e, 100)
        // Tiled across the frame: the strip's blue far to the right of its own 100 pixels.
        let f = frame(e)
        let far = (320 * f.w + 1050) * 4
        #expect(f.px[far + 2] > 200 || f.px[far] > 200)
        // And moving 30 pixels a second to the right.
        let before = try #require(bandStart(e))
        steps(e, 1000)
        let after = try #require(bandStart(e))
        #expect(near(after, before + 30), "\(before) to \(after)")
    }

    // An 8-bit picture with a palette is read, as one of Test Game 83's backgrounds is: two colours, blue and green.
    @Test func anEightBitPictureIsRead() throws {
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        let w = 6, h = 2, row = 8
        put(UInt32(54 + 8 + row * h)); put(0); put(54 + 8)
        put(40); put(UInt32(w)); put(UInt32(h))
        d.append(contentsOf: [1, 0, 8, 0])
        put(0); put(UInt32(row * h)); put(0); put(0); put(2); put(0)
        d.append(contentsOf: [255, 0, 0, 0, 0, 255, 0, 0])
        // Bottom row first: all colour 1, then the top row, colour 0 first.
        d.append(contentsOf: [1, 1, 1, 1, 1, 1, 0, 0])
        d.append(contentsOf: [0, 1, 1, 1, 1, 1, 0, 0])
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\partslot\n".utf8),
            "cg.pak": NekoSDKFlowFixture.archive([("p.bmp", d)]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        var out = [UInt32](repeating: 0, count: 3)
        #expect(vnneko_debug_art_pixels(root.path, "cg\\p.bmp", "", &out))
        // The first pixel, top left, is colour 0: blue and opaque, as red, green, blue and alpha from the top byte.
        #expect(out[0] == 0x0000_FFFF, "\(String(out[0], radix: 16))")
    }
}
