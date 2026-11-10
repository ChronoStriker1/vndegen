import CNekoSDK
import Foundation
import Testing
import TestSupport

// Shared table settings (core/tables/widgettest.inc): `submenudim` on a title, as Test Games 82 and 87's dim their other
// buttons while a submenu is open and close it on the right button; `knob`, Test Game 87's sliders; and `waitscale`, Test
// Game 82's auto wait, which scales waits by a setting of its own.
@Suite struct NekoSDKWidgetTests {
    // A 24-bit BMP, `w` wide, its rows coloured by `colour(row)` as red, green, blue.
    private func bmp(_ w: Int, _ h: Int, _ colour: (Int) -> [UInt8]) -> Data {
        let row = (w * 3 + 3) & ~3
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(54 + row * h)); put(0); put(54)
        put(40); put(UInt32(w)); put(UInt32(h))
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(UInt32(row * h)); put(0); put(0); put(0); put(0)
        // Bottom row first.
        for y in (0..<h).reversed() {
            let rgb = colour(y)
            var line = Data()
            for _ in 0..<w { line.append(contentsOf: [rgb[2], rgb[1], rgb[0]]) }
            line.append(Data(repeating: 0, count: row - w * 3))
            d.append(line)
        }
        return d
    }

    // The button sheet's rows: normal, hover, selected, selected and hovered, disabled.
    private let white: [UInt8] = [255, 255, 255], yellow: [UInt8] = [255, 255, 0], red: [UInt8] = [255, 0, 0],
                magenta: [UInt8] = [255, 0, 255], grey: [UInt8] = [128, 128, 128], green: [UInt8] = [0, 254, 0]

    private func game(_ folder: String = "widgettest", _ more: [String: Data] = [:]) throws -> URL {
        let rows = [white, yellow, red, magenta, grey]
        return try NekoSDKFlowFixture.game(more.merging([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\\(folder)\nFirstFlow\tflow\\t.txt\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("b.bmp", bmp(50, 250) { rows[$0 / 50] }), ("k.bmp", bmp(10, 10) { _ in [0, 254, 0] }),
                                                      // Green all round a red middle, for the colour key; and ten white digits.
                                                      ("g.bmp", bmp(10, 10) { $0 >= 4 && $0 < 6 ? [255, 0, 0] : [0, 255, 0] }),
                                                      ("d.bmp", bmp(100, 10) { _ in [255, 255, 255] })]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 2, next: 3, p: [0: 1000]),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ]) { $1 })
    }

    private func pixel(_ e: OpaquePointer, _ x: Int, _ y: Int) -> [UInt8] {
        for _ in 0..<2 { _ = vnneko_step(e, 16) }
        var w: Int32 = 0, h: Int32 = 0, changed = false
        guard let px = vnneko_frame(e, &w, &h, &changed) else { return [] }
        let at = (y * Int(w) + x) * 4
        return [px[at], px[at + 1], px[at + 2]]
    }

    private func click(_ e: OpaquePointer, _ x: Int, _ y: Int, button: Int32 = 0) {
        vnneko_mouse_move(e, Float(x) / 1280, Float(y) / 720)
        vnneko_mouse_button(e, button, true)
        vnneko_mouse_button(e, button, false)
        for _ in 0..<3 { _ = vnneko_step(e, 16) }
    }

    @Test func aSubmenuDimsTheOtherButtonsAndAKnobFollowsThePointer() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        vnneko_mouse_move(e, 0.9, 0.9)
        #expect(pixel(e, 10, 110) == white)

        // Open: its opener is lit, its own button shows, and the rest are dimmed and do nothing.
        click(e, 10, 10)
        #expect(pixel(e, 10, 10) == red || pixel(e, 10, 10) == magenta)
        #expect(pixel(e, 210, 10) != [0, 0, 0])
        vnneko_mouse_move(e, 0.9, 0.9)
        #expect(pixel(e, 10, 110) == grey)
        click(e, 10, 110)
        #expect(pixel(e, 103, 303) != green)
        #expect(pixel(e, 10, 110) == grey)

        // The right button closes it, and Config opens.
        click(e, 640, 600, button: 1)
        vnneko_mouse_move(e, 0.9, 0.9)
        #expect(pixel(e, 10, 110) == white)
        #expect(pixel(e, 210, 10) == [0, 0, 0])
        click(e, 10, 110)
        #expect(pixel(e, 103, 303) == green)

        // A press on the track puts the knob's middle under the pointer: 20 units, 6 pixels each, from 100.
        #expect(pixel(e, 100, 505) != green)
        click(e, 100 + 6 * 20 + 5, 305)
        #expect(pixel(e, 223, 303) == green)
        #expect(pixel(e, 103, 303) != green)
        #expect(pixel(e, 102, 505) == green)
        #expect(pixel(e, 103, 505) != green)
        // At the track's end it stops at the range's 50.
        click(e, 100 + 6 * 50 + 4, 305)
        #expect(pixel(e, 100 + 6 * 50 + 3, 303) == green)
        #expect(pixel(e, 109, 505) == green)
        click(e, 100, 505)
        #expect(pixel(e, 163, 303) == green)
        #expect(pixel(e, 100, 505) != green)
    }

    // `submenudim hide`: the other buttons aren't drawn while the submenu is open, and still take no press.
    @Test func aSubmenuCanHideTheOtherButtons() throws {
        let root = try game("widgettesthide")
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        vnneko_mouse_move(e, 0.9, 0.9)
        #expect(pixel(e, 10, 110) == white)
        click(e, 10, 10)
        vnneko_mouse_move(e, 0.9, 0.9)
        #expect(pixel(e, 10, 110) == [0, 0, 0])
        // The opener goes too, and a click where it was doesn't close the submenu.
        #expect(pixel(e, 10, 10) == [0, 0, 0])
        click(e, 10, 10)
        vnneko_mouse_move(e, 0.9, 0.9)
        #expect(pixel(e, 210, 10) != [0, 0, 0])
        click(e, 10, 110)
        #expect(pixel(e, 103, 303) != green)
        click(e, 640, 600, button: 1)
        vnneko_mouse_move(e, 0.9, 0.9)
        #expect(pixel(e, 10, 110) == white)
        // The colour key: the green round the picture's red rows is drawn as nothing.
        #expect(pixel(e, 302, 302) == [0, 0, 0])
        #expect(pixel(e, 302, 305) == red)

        // Config: a knob 4.08 pixels a unit, and its value with no leading zeros.
        click(e, 10, 110)
        #expect(pixel(e, 603, 403) == [0, 0, 0])
        #expect(pixel(e, 618, 403) == [0, 0, 0])
        #expect(pixel(e, 633, 403) == white)
        click(e, 100 + 204 + 5, 305)
        #expect(pixel(e, 100 + 204 + 3, 303) == green)
        #expect(pixel(e, 603, 403) == [0, 0, 0])
        #expect(pixel(e, 618, 403) == white)
        #expect(pixel(e, 633, 403) == white)
    }

    // `part selftest` on the sprite gallery: Test Game 88's reader leaves its sc_view.ini alone, here one it would read as
    // a heroine, and the part's square over the screen's picture and under its button, the presses on the
    // square its own and the rest the screen's, and its top layer over the screen's buttons.
    @Test func aTableScreenCanHostAGamePart() throws {
        let root = try game("widgettestpart", ["cfg/sc_view.ini": Data("#p 0 face\nobj\\\\b.bmp\n##\n".utf8)])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        click(e, 10, 10)
        vnneko_mouse_move(e, 0.9, 0.9)
        let blue: [UInt8] = [0, 0, 255]
        #expect(pixel(e, 402, 402) == blue)
        #expect(pixel(e, 407, 407) == white)
        #expect(pixel(e, 5, 5) == white)
        // A controller reaches the part's square too: a step down from above it lands there, and Confirm presses it.
        vnneko_mouse_move(e, 405.0 / 1280, 300.0 / 720)
        #expect(vnneko_move_focus(e, 0, 1))
        vnneko_mouse_button(e, 0, true)
        vnneko_mouse_button(e, 0, false)
        vnneko_mouse_move(e, 0.9, 0.9)
        #expect(pixel(e, 402, 402) == red)
        click(e, 407, 407)
        vnneko_mouse_move(e, 0.9, 0.9)
        #expect(pixel(e, 5, 5) == blue)
        // A press off its square goes to the button there: back to the title, whose Config button sits at 0, 0.
        click(e, 25, 25)
        vnneko_mouse_move(e, 0.9, 0.9)
        #expect(pixel(e, 402, 402) == [0, 0, 0])
        // Opening the screen again tells the part, which starts afresh. Going to the title holds presses a moment first.
        for _ in 0..<20 { _ = vnneko_step(e, 16) }
        click(e, 10, 10)
        vnneko_mouse_move(e, 0.9, 0.9)
        #expect(pixel(e, 402, 402) == blue)
        #expect(pixel(e, 5, 5) == white)
    }

    // `waitscale autowait`, at its default 50: a wait of 1000 ms lasts 500.
    @Test func aWaitScalesByItsOwnSetting() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        #expect(vnneko_debug_press(e, "start"))
        for _ in 0..<3 { _ = vnneko_step(e, 16) }
        let status = { String(cString: vnneko_debug_status(e)) }
        #expect(status().contains("time"), "\(status())")
        // The status names the node after the one waiting: #3 while the first wait runs, then the end, -1, once the second
        // has begun.
        for _ in 0..<22 { _ = vnneko_step(e, 16) }  // about 400 ms in
        #expect(status().contains("s.txt #3 time"), "\(status())")
        for _ in 0..<12 { _ = vnneko_step(e, 16) }  // about 600 ms in
        #expect(status().contains("s.txt #-1 time"), "\(status())")
    }

    // `replaychapters` on the Scene screen: a play_view2.ini line's fourth field is the chapter its card replays, which a
    // game part gives the scenes of, as each of Test Game 96's cards plays its own chapter's file though three share
    // their scripts (set 2). Any of the line's three scripts read unlocks it, here the third, and the replay's end
    // comes back to the Scene screen.
    @Test func aSceneCardReplaysTheChapterItsLineNames() throws {
        let root = try game("widgettestpart", [
            // Two cards with the same scripts and chapters of their own, one to a page.
            "cfg/play_view2.ini": Data("0,obj\\k.bmp,-1,7,scr\\\\a.txt,src\\\\b.txt,scr\\\\k.txt\n0,obj\\k.bmp,-1,8,scr\\\\a.txt,src\\\\b.txt,scr\\\\k.txt\n".utf8),
            // The test part's chapter 7 is this one scene, and its chapter 8 the other.
            "scr/c7.txt": NekoSDKFlowFixture.flow([.init(op: 0, next: 2), .init(op: 2, next: 0, p: [0: 1000])]),
            "scr/c8.txt": NekoSDKFlowFixture.flow([.init(op: 0, next: 2), .init(op: 2, next: 0, p: [0: 1000])]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data"), save = data.appendingPathComponent("AppData/Roaming/vndegen/widgettestpart/save")
        try FileManager.default.createDirectory(at: save, withIntermediateDirectories: true)
        try Data("scr\\k.txt\n".utf8).write(to: save.appendingPathComponent("vndegen_seen.txt"))
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(e) }
        let status = { String(cString: vnneko_debug_status(e)) }
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        #expect(vnneko_debug_press(e, "scene"))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        // The part's square under the cards, Layer::MenuUnder: blue 20 across at 300, 300, over the screen's white
        // picture and under the card's green thumbnail, 10 across.
        #expect(pixel(e, 305, 305) == green)
        #expect(pixel(e, 315, 305) == [0, 0, 255])
        #expect(pixel(e, 315, 315) == [0, 0, 255])
        #expect(pixel(e, 350, 315) == white)
        // The second tab has no scene and Music is in the table's `buttonsoff`: both are drawn disabled and take no
        // press, as Test Game 90's bonus release draws its empty tabs and its Music. The first tab is as it was.
        vnneko_mouse_move(e, 0.9, 0.9)
        #expect(pixel(e, 925, 25) == grey)
        #expect(pixel(e, 1025, 25) == grey)
        #expect(pixel(e, 825, 25) != grey)
        click(e, 925, 25)
        click(e, 1025, 25)
        vnneko_mouse_move(e, 0.9, 0.9)
        #expect(status().contains(" menu scene"), "\(status())")
        #expect(pixel(e, 305, 305) == green)
        click(e, 320, 315)
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        #expect(status().hasPrefix("scr\\c7.txt"), "\(status())")
        for _ in 0..<90 { _ = vnneko_step(e, 16) }
        #expect(status().contains(" menu scene"), "\(status())")
        // The second page's card has the same scripts and plays its own chapter. `replaykeeps`: its replay comes back to
        // that page, so the card under the pointer is still the second.
        for round in 0..<2 {
            for _ in 0..<30 { _ = vnneko_step(e, 16) }
            if !status().contains(" menu scene") { break }
            if round == 0 { #expect(vnneko_debug_press(e, "page1")) }
            for _ in 0..<5 { _ = vnneko_step(e, 16) }
            click(e, 320, 315)
            for _ in 0..<5 { _ = vnneko_step(e, 16) }
            #expect(status().hasPrefix("scr\\c8.txt"), "\(status())")
            for _ in 0..<90 { _ = vnneko_step(e, 16) }
            #expect(status().contains(" menu scene"), "\(status())")
        }
    }

    @Test func aReplayOfOneSceneEndsAtTheSceneScreen() throws {
        let root = try game("widgettesttabs", [
            "cfg/play_view2.ini": Data("0,obj\\k.bmp,-1,7,scr\\\\k.txt,0,0\n".utf8),
            "scr/k.txt": NekoSDKFlowFixture.flow([.init(op: 0, next: 2), .init(op: 2, next: 0, p: [0: 500])]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data"), save = data.appendingPathComponent("AppData/Roaming/vndegen/widgettesttabs/save")
        try FileManager.default.createDirectory(at: save, withIntermediateDirectories: true)
        try Data("scr\\k.txt\n".utf8).write(to: save.appendingPathComponent("vndegen_seen.txt"))
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(e) }
        let status = { String(cString: vnneko_debug_status(e)) }
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        #expect(vnneko_debug_press(e, "scene"))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        click(e, 320, 315)
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        #expect(status().contains("k.txt #"), "\(status())")
        for _ in 0..<90 { _ = vnneko_step(e, 16) }
        #expect(status().contains(" menu scene"), "\(status())")
    }

    @Test func anEmptyTabOpensWithoutTheKey() throws {
        let root = try game("widgettesttabs", [
            "cfg/play_view2.ini": Data("0,obj\\k.bmp,-1,7,scr\\\\k.txt,0,0\n".utf8),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data"), save = data.appendingPathComponent("AppData/Roaming/vndegen/widgettesttabs/save")
        try FileManager.default.createDirectory(at: save, withIntermediateDirectories: true)
        try Data("scr\\k.txt\n".utf8).write(to: save.appendingPathComponent("vndegen_seen.txt"))
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(e) }
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        #expect(vnneko_debug_press(e, "scene"))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        vnneko_mouse_move(e, 0.9, 0.9)
        #expect(pixel(e, 305, 305) == green)
        #expect(pixel(e, 925, 25) == white)
        click(e, 925, 25)
        vnneko_mouse_move(e, 0.9, 0.9)
        #expect(pixel(e, 305, 305) != green)
    }
}
