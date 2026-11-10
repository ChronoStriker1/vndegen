import CNekoSDK
import Foundation
import Testing
import TestSupport

// Test Game 82's save cards: the chapter's title picked by the save's S5000 (`slotchaptervar`), each part of the date at a place
// of its own with a blank for a leading zero (`datefield`), a frame around the thumbnail (`slotthumbframe`) and card
// numbers 15 apart from 14-wide digits (`slotdigits`' step).
@Suite struct NekoSDKCardFieldTests {
    // A BMP of w by h whose pixel at x, y is color(x, y), as blue, green, red.
    private func bmp(_ w: Int, _ h: Int, _ color: (Int, Int) -> [UInt8]) -> Data {
        let row = (w * 3 + 3) & ~3
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(54 + row * h)); put(0); put(54)
        put(40); put(UInt32(w)); put(UInt32(h))
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(UInt32(row * h)); put(0); put(0); put(0); put(0)
        for y in (0..<h).reversed() {
            var line = Data()
            for x in 0..<w { line.append(contentsOf: color(x, y)) }
            line.append(Data(repeating: 0, count: row - w * 3))
            d.append(line)
        }
        return d
    }

    private func pixel(_ e: OpaquePointer, _ x: Int, _ y: Int) -> [UInt8] {
        var w: Int32 = 0, h: Int32 = 0, changed = false
        guard let px = vnneko_frame(e, &w, &h, &changed) else { return [] }
        let at = (y * Int(w) + x) * 4
        return [px[at], px[at + 1], px[at + 2]]
    }

    @Test(arguments: ["emptycards", "emptycardsnumber"]) func anEmptyCardsNumberFadesWithItOnlyWhereTheTableSays(folder: String) throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\\(folder)\nFirstFlow\tflow\\t.txt\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([
                ("c.bmp", bmp(482, 555) { _, _ in [200, 0, 0] }),
                ("d.bmp", bmp(140, 20) { x, _ in [0, 0, UInt8(20 * (x / 14) + 10)] }),
            ]),
            "flow/t.txt": NekoSDKFlowFixture.flow([.init(op: 0, next: 0)]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_press(e, "load"))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        let card = pixel(e, 60 + 100, 99 + 60), number = pixel(e, 60 + 259 + 7, 99 + 6 + 10)
        #expect(card[2] > 60 && card[2] < 140, "\(card)")
        if folder == "emptycards" {
            #expect(number == [30, 0, 0], "\(number)")
        } else {
            #expect(number[0] > 5 && number[0] < 25 && number[2] > 20, "\(number)")
        }
    }

    @Test func aCardDrawsItsChapterDateFrameAndNumberWhereTheTableSays() throws {
        // The card is blue; each digit's red is 20 times it plus 10; each chapter cell's green is 60 times it plus 30.
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\cardtest\nFirstFlow\tflow\\t.txt\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([
                ("c.bmp", bmp(482, 555) { _, _ in [200, 0, 0] }),
                ("d.bmp", bmp(140, 20) { x, _ in [0, 0, UInt8(20 * (x / 14) + 10)] }),
                ("n.bmp", bmp(342, 22) { x, _ in [0, UInt8(60 * (x / 114) + 30), 0] }),
                ("f.bmp", bmp(158, 90) { _, _ in [0, 250, 250] }),
            ]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                // S5000 = 2.
                .init(op: 6, next: 3, p: [0: 1, 1: 0, 2: 5000, 4: 2, 5: 2]),
                .init(op: 1, next: 0, s: [1: "scr\\s.txt"]),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([.init(op: 0, next: 2), .init(op: 2, next: 0, p: [0: 100_000])]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(vnneko_debug_jump(first, "flow\\t.txt", 0))
        for _ in 0..<10 { _ = vnneko_step(first, 16) }
        #expect(vnneko_debug_variable(first, 1, 5000) == 2)
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)

        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_press(e, "load"))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        let now = Calendar.current.dateComponents([.year, .month, .day], from: Date())
        func digitAt(_ x: Int, _ y: Int) -> Int? {
            let p = pixel(e, x, y)
            return p == [0, 0, 200] ? nil : (Int(p[0]) - 10) / 20
        }
        // Card 1 at 60, 99: the number 001, 15 apart.
        #expect([229, 244, 259].map { digitAt(60 + $0 + 7, 99 + 6 + 10) } == [0, 0, 1])
        // The year's four digits from 251, 28.
        let year = String(now.year!).compactMap { $0.wholeNumberValue }
        #expect((0..<4).map { digitAt(60 + 251 + 15 * $0 + 7, 99 + 28 + 10) } == year.map { Optional($0) })
        // The month and day right-aligned in two places, the first blank below 10.
        for (value, x) in [(now.month!, 347), (now.day!, 406)] {
            #expect(digitAt(60 + x + 15 + 7, 99 + 28 + 10) == value % 10)
            #expect(digitAt(60 + x + 7, 99 + 28 + 10) == (value >= 10 ? value / 10 : nil))
        }
        // `datemark 9 330 28`: the date sheet's cell 9 at its own place between the year and the month.
        #expect(digitAt(60 + 330 + 7, 99 + 28 + 10) == 9)
        // `slotcommentbutton`: the button at 200, 60 on the card, whose press asks the host for the card's comment.
        #expect(pixel(e, 60 + 200 + 5, 99 + 60 + 5) == [250, 250, 0])
        vnneko_mouse_move(e, Float(60 + 200 + 5) / 1280, Float(99 + 60 + 5) / 720)
        _ = vnneko_step(e, 16)
        vnneko_mouse_button(e, 0, true)
        vnneko_mouse_button(e, 0, false)
        _ = vnneko_step(e, 16)
        var comment = [CChar](repeating: 0, count: 64)
        #expect(vnneko_take_comment_request(e, &comment, comment.count) == 1)
        // The chapter from S5000: cell 2, green 150.
        #expect(pixel(e, 60 + 350 + 5, 99 + 6 + 5) == [0, 150, 0])
        // The frame's corner, outside the thumbnail.
        #expect(pixel(e, 60 + 27, 99 + 8) == [250, 250, 0])
        // Card 2 has no save: no frame and no chapter.
        #expect(pixel(e, 570 + 27, 99 + 8) == [0, 0, 200])
        #expect(pixel(e, 570 + 350 + 5, 99 + 6 + 5) == [0, 0, 200])
    }

    // Test Game 82's dialogs take their question from a strip of seven titles (`question FILE X Y COLS COL`), drawn over
    // the frame the table names: quit's is the strip's third cell, here green 150, over a frame of yellow.
    @Test func aDialogQuestionIsACellOfAStripOverItsFrame() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\cardtest\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([
                ("n.bmp", bmp(342, 22) { x, _ in [0, UInt8(60 * (x / 114) + 30), 0] }),
                ("f.bmp", bmp(158, 90) { _, _ in [0, 250, 250] }),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_press(e, "quit"))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        #expect(pixel(e, 315, 315) == [0, 150, 0])
        #expect(pixel(e, 440, 380) == [250, 250, 0])
    }

    // Test Game 82's cg_view.ini lists pictures as `ID,PICTURE`; one seen in cfg.sav unlocks its card, whose thumbnail gets
    // the frame `cellthumbframe` names, as the PC draws d004_001 over every unlocked card.
    @Test func aCardFromAnIdPictureLineShowsWithItsThumbnailFrame() throws {
        var cfg = Data(count: 104_000)
        cfg[48_000] = 1
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\cardtest\n".utf8),
            "cfg/cg_view.ini": Data("#p 0 obj\\t.bmp none\n0,cg\\a.bmp\n##\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([
                ("t.bmp", bmp(40, 30) { _, _ in [0, 0, 250] }),
                ("f.bmp", bmp(44, 34) { x, y in x < 2 || y < 2 ? [250, 250, 0] : [0, 250, 0] }),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data"), save = data.appendingPathComponent("AppData/Roaming/vndegen/cardtest/save")
        try FileManager.default.createDirectory(at: save, withIntermediateDirectories: true)
        try NekoSDKFlowFixture.packed(cfg).write(to: save.appendingPathComponent("cfg.sav"))
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_press(e, "cg"))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        // The frame's edge at the card plus 8, then the frame's inside over the thumbnail.
        #expect(pixel(e, 108, 108) == [0, 250, 250])
        #expect(pixel(e, 120, 120) == [0, 250, 0])
    }

    @Test func aClickOnAnOpenPictureIsNotAPressOnTheButtonUnderIt() throws {
        var cfg = Data(count: 104_000)
        cfg[48_000] = 1
        cfg[48_004] = 1
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\cardtest\n".utf8),
            "cfg/cg_view.ini": Data("#p 0 obj\\t.bmp none\n0,cg\\a.bmp\n1,cg\\b.bmp\n##\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([
                ("t.bmp", bmp(40, 30) { _, _ in [0, 0, 250] }),
                ("f.bmp", bmp(44, 34) { _, _ in [0, 250, 0] }),
            ]),
            "cg.pak": NekoSDKFlowFixture.archive([
                ("a.bmp", bmp(1280, 720) { _, _ in [250, 0, 0] }),
                ("b.bmp", bmp(1280, 720) { _, _ in [250, 0, 250] }),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data"), save = data.appendingPathComponent("AppData/Roaming/vndegen/cardtest/save")
        try FileManager.default.createDirectory(at: save, withIntermediateDirectories: true)
        try NekoSDKFlowFixture.packed(cfg).write(to: save.appendingPathComponent("cfg.sav"))
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(e) }
        let status = { String(cString: vnneko_debug_status(e)) }
        func click(_ x: Int, _ y: Int) {
            vnneko_mouse_move(e, Float(x) / 1280, Float(y) / 720)
            _ = vnneko_step(e, 16)
            vnneko_mouse_button(e, 0, true)
            vnneko_mouse_button(e, 0, false)
            for _ in 0..<3 { _ = vnneko_step(e, 16) }
        }
        #expect(vnneko_debug_press(e, "cg"))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        #expect(pixel(e, 610, 610) == [0, 250, 0])
        click(120, 120)
        #expect(pixel(e, 610, 610) == [0, 0, 250], "\(pixel(e, 610, 610))")
        for _ in 0..<4 where pixel(e, 610, 610) != [0, 250, 0] {
            click(610, 610)
            #expect(status().contains(" menu cg"), "\(status())")
        }
        #expect(pixel(e, 610, 610) == [0, 250, 0])
        click(610, 610)
        #expect(status().contains(" menu scene"), "\(status())")
    }

    // `pic FILE X Y turn DEGREES` turns a picture about its middle by the screen's clock, as Test Game 83's title turns
    // its lace circles: a bar 100 by 20 at 1100, 600 lies across at first, and stands upright a second later at 90 a second.
    @Test func aTitlePictureTurnsAboutItsMiddle() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\cardtest\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("spin.bmp", bmp(100, 20) { _, _ in [0, 0, 250] })]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<2 { _ = vnneko_step(e, 16) }
        // 40 right of its middle, 1150, 610, and 40 below it.
        #expect(pixel(e, 1190, 610) == [250, 0, 0])
        #expect(pixel(e, 1150, 650) != [250, 0, 0])
        for _ in 0..<60 { _ = vnneko_step(e, 16) }
        _ = vnneko_step(e, 8)
        #expect(pixel(e, 1190, 610) != [250, 0, 0])
        #expect(pixel(e, 1150, 650) == [250, 0, 0])
    }

    // `pic FILE X Y scale 50` draws a picture at half size with its top left at X, Y, each pixel the mean of a 2 by 2
    // block, as Test Game 90 shows one gallery picture (set 1). The picture's blue alternates across and its green down,
    // so every block averages to the same half of each, and its right half is red.
    @Test func aPictureDrawsAtHalfSizeAveraged() throws {
        let half = bmp(40, 40) { x, y in [x % 2 == 0 ? 0 : 200, y % 2 == 0 ? 0 : 100, x >= 20 ? 240 : 0] }
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\cardtest\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("half.bmp", half)]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<2 { _ = vnneko_step(e, 16) }
        #expect(pixel(e, 40, 40) == [0, 50, 100])
        #expect(pixel(e, 49, 59) == [0, 50, 100])
        #expect(pixel(e, 50, 45) == [240, 50, 100])
        #expect(pixel(e, 59, 59) == [240, 50, 100])
        // Twenty across and down and no more.
        #expect(pixel(e, 60, 45) == [0, 0, 0])
        #expect(pixel(e, 45, 60) == [0, 0, 0])
    }

    // Test Game 90's play_view.ini lines are `TAB THUMB STEP NUMBER SCRIPT`: the scene's card shows its thumbnail once
    // that script has been read, and not before.
    @Test(arguments: [false, true]) func aSceneFromAFiveFieldLineUnlocksByItsScript(read: Bool) throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\cardtest\n".utf8),
            "cfg/play_view.ini": Data("//tab\tthumb\tstep\tnumber\tscript\n0\tobj\\t.bmp\t-1\t1000\tscr\\ep_001.txt\t//one\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("t.bmp", bmp(40, 30) { _, _ in [0, 0, 250] })]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data"), save = data.appendingPathComponent("AppData/Roaming/vndegen/cardtest/save")
        try FileManager.default.createDirectory(at: save, withIntermediateDirectories: true)
        if read { try Data("scr\\ep_001.txt\n".utf8).write(to: save.appendingPathComponent("vndegen_seen.txt")) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_press(e, "scene"))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        #expect((pixel(e, 320, 315) == [250, 0, 0]) == read)
    }
}
