import CNekoSDK
import Foundation
import Testing
import TestSupport

// The older layout's commands (core/oldlayout.cpp): a background at the top left, a standing picture at one of six places,
// halfway down and moved by parameters 2 and 3, each shown by the next update, and the deletes, as Test Game 83's PC saves
// place them.
@Suite struct NekoSDKOldLayoutTests {
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
        var w: Int32 = 0, h: Int32 = 0, changed = false
        guard let px = vnneko_frame(e, &w, &h, &changed) else { return [] }
        let at = (y * Int(w) + x) * 4
        return [px[at], px[at + 1], px[at + 2]]
    }

    @Test func placesSlotsAndDeletes() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("FirstFlow\tflow\\t.txt\n".utf8),
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
                // Place 3, its middle 400 left of the screen's, moved 5 right and 7 down.
                .init(op: 13, next: 4, p: [0: 3, 2: 5, 3: 7], s: [1: "sc\\r.bmp"]),
                .init(op: 9, next: 5),
                .init(op: 2, next: 6, p: [0: 1000]),
                .init(op: 14, next: 7, p: [0: 3]),
                .init(op: 12, next: 8),
                .init(op: 9, next: 9),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        // (1280 - 101) / 2 - 400 + 5 = 194, and (720 - 81) / 2 + 7 = 326.
        #expect(pixel(e, 0, 0) == [0, 255, 0])
        #expect(pixel(e, 194, 326) == [255, 0, 0])
        #expect(pixel(e, 294, 406) == [255, 0, 0])
        #expect(pixel(e, 193, 326) != [255, 0, 0])
        #expect(pixel(e, 194, 325) != [255, 0, 0])
        #expect(pixel(e, 295, 326) != [255, 0, 0])
        for _ in 0..<100 { _ = vnneko_step(e, 16) }
        #expect(pixel(e, 0, 0) != [0, 255, 0])
        #expect(pixel(e, 194, 326) != [255, 0, 0])
    }

    // `layoutpairs`: Test Game 96's PC saves hold each place's picture twice, the lit one in the slot after the place's
    // own, and the story's commands know only the place. Here command 15 stands in for a loaded save's lit pictures, in
    // slots 601 and 101: taking place 3's picture away takes 601's with it, and a new background takes 101's.
    @Test(arguments: [true, false]) func aPlacesSecondSlotGoesWithIt(pairs: Bool) throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\\(pairs ? "layoutpairs" : "layoutold")\nFirstFlow\tflow\\t.txt\n".utf8),
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
                .init(op: 13, next: 4, p: [0: 3], s: [1: "sc\\r.bmp"]),
                .init(op: 15, next: 5, p: [1: 900, 2: 100, 3: 601], s: [1: "sc\\r.bmp"]),
                .init(op: 15, next: 6, p: [1: 900, 2: 300, 3: 101], s: [1: "sc\\r.bmp"]),
                .init(op: 9, next: 7),
                .init(op: 2, next: 8, p: [0: 1000]),
                .init(op: 14, next: 9, p: [0: 3]),
                .init(op: 10, next: 10, s: [1: "bg\\g.bmp"]),
                .init(op: 9, next: 11),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        let red: [UInt8] = [255, 0, 0]
        #expect(pixel(e, 189, 319) == red)
        #expect(pixel(e, 950, 140) == red)
        #expect(pixel(e, 950, 340) == red)
        if pairs {
            // A save holds each pair with the slot after first, as 96's PC writes them: of the three records naming
            // the standing picture, 101 (before the background's 100), then 601, then 600. The slot is 12 bytes before
            // the name's length in this layout's records.
            #expect(vnneko_debug_save(e, 1))
            let saves = root.appendingPathComponent("data/AppData/Roaming/vndegen/layoutpairs/save")
            let file = try #require(try FileManager.default.contentsOfDirectory(at: saves, includingPropertiesForKeys: nil)
                .first { $0.lastPathComponent.lowercased() == "save001.sav" })
            let save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: file))
            var slots: [Int32] = [], from = save.startIndex
            while let found = save.range(of: Data("sc\\r.bmp".utf8), in: from..<save.endIndex) {
                let at = found.lowerBound - 12
                slots.append(save.subdata(in: at..<(at + 4)).withUnsafeBytes { $0.loadUnaligned(as: Int32.self) })
                from = found.upperBound
            }
            #expect(slots == [101, 601, 600], "\(slots)")
        }
        for _ in 0..<100 { _ = vnneko_step(e, 16) }
        #expect(pixel(e, 0, 0) == [0, 255, 0])
        #expect(pixel(e, 189, 319) != red)
        // Without the setting the slots after are no one's, and stay.
        #expect((pixel(e, 950, 140) == red) == !pairs)
        #expect((pixel(e, 950, 340) == red) == !pairs)
    }

    // Test Game 87 picks a standing picture's slot by parameter 1 (`layoutslotby p1`), and 14 deletes by that number.
    @Test func slotsByParameter1AndDeletesBySlot() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\layouttest\nFirstFlow\tflow\\t.txt\n".utf8),
            "sc.pak": NekoSDKFlowFixture.archive([("r.bmp", bmp(101, 81, [0, 0, 255]))]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                // 位置:1_2, slot 400, its middle 256 left of the screen's; 位置:3_0, slot 200, 256 right.
                .init(op: 13, next: 3, p: [0: 1, 1: 2], s: [1: "sc\\r.bmp"]),
                .init(op: 13, next: 4, p: [0: 3], s: [1: "sc\\r.bmp"]),
                .init(op: 9, next: 5),
                .init(op: 2, next: 6, p: [0: 1000]),
                // 位置:2 deletes slot 400, the first, though no picture is at place 2.
                .init(op: 14, next: 7, p: [0: 2]),
                .init(op: 9, next: 8),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        // (1280 - 101) / 2 = 589, and 589 - 256 = 333, 589 + 256 = 845; (720 - 81) / 2 = 319.
        #expect(pixel(e, 333, 319) == [255, 0, 0])
        #expect(pixel(e, 845, 319) == [255, 0, 0])
        for _ in 0..<100 { _ = vnneko_step(e, 16) }
        #expect(pixel(e, 333, 319) != [255, 0, 0])
        #expect(pixel(e, 845, 319) == [255, 0, 0])
    }

    // Test Game 82 keeps its CGs in slot 102 (`layoutcgslot`), and a CG takes the background away, and a background the CG.
    @Test func aCGSlotOfItsOwnTakesTheBackgroundAway() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\layouttest\nFirstFlow\tflow\\t.txt\n".utf8),
            "bg.pak": NekoSDKFlowFixture.archive([("g.bmp", bmp(8, 8, [0, 255, 0]))]),
            "cg.pak": NekoSDKFlowFixture.archive([("r.bmp", bmp(4, 4, [0, 0, 255]))]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 10, next: 3, s: [1: "bg\\g.bmp"]),
                .init(op: 11, next: 4, s: [1: "cg\\r.bmp"]),
                .init(op: 9, next: 5),
                .init(op: 2, next: 6, p: [0: 1000]),
                .init(op: 10, next: 7, s: [1: "bg\\g.bmp"]),
                .init(op: 9, next: 8),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        // The CG over where the background was, and none of the background, the larger, past it.
        #expect(pixel(e, 0, 0) == [255, 0, 0])
        #expect(pixel(e, 6, 6) != [0, 255, 0])
        for _ in 0..<100 { _ = vnneko_step(e, 16) }
        #expect(pixel(e, 0, 0) == [0, 255, 0])
    }

    @Test(arguments: [0, 1, 2]) func theOutfitSwitchChangesAStandingPicturesOutfit(choice: Int) throws {
        let red: [UInt8] = [0, 0, 255], green: [UInt8] = [0, 255, 0], blue: [UInt8] = [255, 0, 0]
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\outfittest\nFirstFlow\tflow\\t.txt\n".utf8),
            "sc.pak": NekoSDKFlowFixture.archive([
                ("sc_000_000_000_000_000.bmp", bmp(10, 10, red)), ("sc_000_003_000_000_000.bmp", bmp(10, 10, green)),
                ("sc_000_004_000_000_000.bmp", bmp(10, 10, blue)),
                ("sc_000_101_000_000_000.bmp", bmp(10, 10, red)), ("sc_000_103_000_000_000.bmp", bmp(10, 10, green)),
                ("sc_000_104_000_000_000.bmp", bmp(10, 10, blue)),
                ("sc_001_000_000_000_000.bmp", bmp(10, 10, red)), ("sc_001_002_000_000_000.bmp", bmp(10, 10, green)),
                ("sc_009_000_000_000_000.bmp", bmp(10, 10, red)), ("sc_009_003_000_000_000.bmp", bmp(10, 10, green)),
            ]),
            "ac.pak": NekoSDKFlowFixture.archive([
                ("ac_000_000_000_000_000.bmp", bmp(10, 10, red)), ("ac_000_003_000_000_000.bmp", bmp(10, 10, green)),
                ("ac_000_004_000_000_000.bmp", bmp(10, 10, blue)),
            ]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 15, next: 3, p: [1: 0, 3: 200], s: [1: "sc\\sc_000_000_000_000_000.bmp"]),
                .init(op: 15, next: 4, p: [1: 20, 3: 201], s: [1: "sc\\sc_000_101_000_000_000.bmp"]),
                .init(op: 15, next: 5, p: [1: 40, 3: 202], s: [1: "sc\\sc_001_000_000_000_000.bmp"]),
                .init(op: 15, next: 6, p: [1: 60, 3: 203], s: [1: "sc\\sc_009_000_000_000_000.bmp"]),
                .init(op: 15, next: 7, p: [1: 80, 3: 204], s: [1: "ac\\ac_000_000_000_000_000.bmp"]),
                .init(op: 13, next: 8, p: [0: 3], s: [1: "sc\\sc_000_000_000_000_000.bmp"]),
                .init(op: 9, next: 9),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data"), saves = data.appendingPathComponent("AppData/Roaming/vndegen/outfittest/save")
        try FileManager.default.createDirectory(at: saves, withIntermediateDirectories: true)
        try Data("outfit \(choice)\n".utf8).write(to: saves.appendingPathComponent("vndegen_settings.txt"))
        let shown: [[UInt8]] = [[255, 0, 0], [0, 255, 0], [0, 0, 255]]
        func check(_ e: OpaquePointer) {
            #expect(pixel(e, 5, 5) == shown[choice])
            #expect(pixel(e, 25, 5) == shown[choice])
            #expect(pixel(e, 45, 5) == (choice == 1 ? shown[1] : shown[0]))
            #expect(pixel(e, 65, 5) == shown[0])
            #expect(pixel(e, 85, 5) == shown[choice])
            #expect(pixel(e, 240, 360) == shown[choice])
        }
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        check(e)
        #expect(vnneko_debug_save(e, 1))
        vnneko_stop(e)
        let file = try #require(try FileManager.default.contentsOfDirectory(at: saves, includingPropertiesForKeys: nil)
            .first { $0.lastPathComponent.lowercased() == "save001.sav" })
        let save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: file))
        #expect(save.range(of: Data("sc\\sc_000_000_000_000_000.bmp".utf8)) != nil)
        #expect(save.range(of: Data("sc\\sc_000_003_000_000_000.bmp".utf8)) == nil)
        #expect(save.range(of: Data("sc\\sc_000_004_000_000_000.bmp".utf8)) == nil)
        let again = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(again) }
        #expect(vnneko_debug_load(again, 1))
        for _ in 0..<3 { _ = vnneko_step(again, 16) }
        check(again)
    }

    @Test func theOutfitSwitchRedressesAPictureOnScreen() throws {
        let red: [UInt8] = [0, 0, 255], green: [UInt8] = [0, 255, 0]
        let inner = ["#inner Config", "obj\\b.bmp", "1", "1000,600", "1,5", "0,0", "0,1", "0,2", "0,3", "0,4", "none", "0,0", "1,1"]
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\outfittest\nFirstFlow\tflow\\t.txt\n".utf8),
            "cfg/msg_button.ini": Data((inner.joined(separator: "\n") + "\n##\n").utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("b.bmp", bmp(50, 250, [200, 200, 200]))]),
            "sc.pak": NekoSDKFlowFixture.archive([
                ("sc_000_000_000_000_000.bmp", bmp(10, 10, red)), ("sc_000_003_000_000_000.bmp", bmp(10, 10, green)),
            ]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 15, next: 3, p: [1: 0, 3: 200], s: [1: "sc\\sc_000_000_000_000_000.bmp"]),
                .init(op: 9, next: 4),
                .init(op: 5, next: 5, s: [2: "x"]),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<20 { _ = vnneko_step(e, 16) }
        #expect(pixel(e, 5, 5) == [255, 0, 0])
        #expect(vnneko_debug_press(e, "Config"))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        vnneko_mouse_move(e, 225.0 / 1280, 125.0 / 720)
        vnneko_mouse_button(e, 0, true)
        vnneko_mouse_button(e, 0, false)
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        #expect(vnneko_debug_press(e, "back"))
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        #expect(pixel(e, 5, 5) == [0, 255, 0])
    }

    @Test(arguments: [(true, UInt8(100)), (false, UInt8(100)), (true, UInt8(255))]) func theBulbFlickersBetweenAPictureAndItsDarkerTwin(on: Bool, blue: UInt8) throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\layoutpairs\nFirstFlow\tflow\\t.txt\n".utf8),
            "cfg/light_info.ini": Data("#p 0\r\nlight,30,0\r\nwait,60\r\nlight,30,255\r\nwait,60\r\n##\r\n".utf8),
            "bg.pak": NekoSDKFlowFixture.archive([("bg_000.bmp", bmp(10, 10, [0, 0, 255])), ("bg_001.bmp", bmp(10, 10, [blue, 0, 0]))]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 10, next: 3, s: [1: "bg\\bg_000.bmp"]),
                .init(op: 9, next: 4),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data"), saves = data.appendingPathComponent("AppData/Roaming/vndegen/layoutpairs/save")
        try FileManager.default.createDirectory(at: saves, withIntermediateDirectories: true)
        try Data("bulb \(on ? 1 : 0)\n".utf8).write(to: saves.appendingPathComponent("vndegen_settings.txt"))
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(e) }
        let lit: [UInt8] = [255, 0, 0], dark: [UInt8] = [0, 0, blue], flickers = on && blue < 255
        func at(_ ms: Int) -> [UInt8] {
            for _ in 0..<(ms / 10) { _ = vnneko_step(e, 10) }
            return pixel(e, 5, 5)
        }
        #expect(at(1000) == (flickers ? dark : lit))
        #expect(at(1500) == lit)
        #expect(at(1500) == (flickers ? dark : lit))
        let middle = at(650)
        #expect(flickers ? (middle != lit && middle != dark) : middle == lit)
    }
}
