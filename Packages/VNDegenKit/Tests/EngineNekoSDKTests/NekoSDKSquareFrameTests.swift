import CNekoSDK
import Foundation
import Testing
import TestSupport

// A game whose init.ini names a MsgWinSquare picture draws a 4:3 frame, as Test Game 88's PC does in a 1280x960 window:
// the 16:9 frame on top, the picture under it, and the message window moved down onto the picture, 2 rows further than
// MsgWinSquareMovement. It's chosen by the host's view, with room either side of the middle, unless Config's
// `screensize` fixes the shape.
@Suite struct NekoSDKSquareFrameTests {
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

    private func game(_ folder: String = "squaretest") throws -> URL {
        try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("""
                UserDataDirectory\tvndegen\\\(folder)
                FirstFlow\tflow\\t.txt
                MsgWinFilename\tobj\\win.bmp
                MsgWinPosY\t480
                MsgWinSquareFilename\tobj\\sq.bmp
                MsgWinSquarePosY\t720
                MsgWinSquareMovement\t240

                """.utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("win.bmp", bmp(1280, 241, [0, 0, 255])), ("sq.bmp", bmp(1280, 240, [255, 0, 0]))]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 5, next: 3, s: [2: "hello"]),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
    }

    // A run with these lines in the game's settings, the window opaque so its colour shows as it is.
    private func start(_ root: URL, _ settings: String = "", folder: String = "squaretest") throws -> OpaquePointer {
        let data = root.appendingPathComponent("data")
        let saves = data.appendingPathComponent("AppData/Roaming/vndegen/\(folder)/save")
        try FileManager.default.createDirectory(at: saves, withIntermediateDirectories: true)
        try Data("opacity 100\n\(settings)".utf8).write(to: saves.appendingPathComponent("vndegen_settings.txt"))
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<20 { _ = vnneko_step(e, 16) }
        return e
    }

    private struct Frame { var w = 0, h = 0; var px: [UInt8] = [] }

    private func frame(_ e: OpaquePointer, view: (Float, Float)) -> Frame {
        vnneko_set_view_size(e, view.0, view.1)
        for _ in 0..<3 { _ = vnneko_step(e, 16) }
        var w: Int32 = 0, h: Int32 = 0, changed = false
        guard let px = vnneko_frame(e, &w, &h, &changed) else { return Frame() }
        return Frame(w: Int(w), h: Int(h), px: Array(UnsafeBufferPointer(start: px, count: Int(w) * Int(h) * 4)))
    }

    private func pixel(_ f: Frame, _ x: Int, _ y: Int) -> [UInt8] {
        let at = (y * f.w + x) * 4
        return [f.px[at], f.px[at + 1], f.px[at + 2]]
    }

    @Test func theViewPicksTheShapeAndTheWindowMovesDown() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try start(root)
        defer { vnneko_stop(e) }

        let wide = frame(e, view: (1600, 900))
        #expect(wide.h == 720)
        #expect(pixel(wide, 10, 600) == [255, 0, 0])

        let square = frame(e, view: (1024, 768))
        #expect(square.w == 1280 && square.h == 960)
        // The scene has no window over it, the picture is under, and the window starts 242 rows down from its 16:9 place.
        #expect(pixel(square, 10, 600) != [255, 0, 0])
        #expect(pixel(square, 10, 721) == [0, 0, 255])
        #expect(pixel(square, 10, 722) == [255, 0, 0])
        #expect(pixel(square, 10, 959) == [255, 0, 0])

        // Near the middle the shape stays as it was, either way.
        #expect(frame(e, view: (1550, 1000)).h == 960)
        #expect(frame(e, view: (1700, 1000)).h == 720)
        #expect(frame(e, view: (1550, 1000)).h == 720)
    }

    // `squarewindow 0`: Test Game 83's PC puts the window exactly as far down as the layer moves, 240 rows, where
    // Test Game 88's, the default, sits two rows lower.
    @Test func theWindowCanSitExactlyWhereTheLayerMoves() throws {
        let root = try game("squaretest0")
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try start(root, folder: "squaretest0")
        defer { vnneko_stop(e) }
        let square = frame(e, view: (1024, 768))
        #expect(square.w == 1280 && square.h == 960)
        #expect(pixel(square, 10, 719) != [255, 0, 0])
        #expect(pixel(square, 10, 720) == [255, 0, 0])
    }

    // Config's sizes fix the shape whatever the view: 1 to 3 for 16:9 and 4 to 6 for 4:3.
    @Test func configsSizeFixesTheShape() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        for (size, view, height) in [(4, (Float(1600), Float(900)), 960), (2, (Float(1024), Float(768)), 720)] {
            let e = try start(root, "screensize \(size)\n")
            #expect(frame(e, view: view).h == height, "screensize \(size)")
            vnneko_stop(e)
        }
    }

    // A sheet of five rows, each its own colour (blue, green, red): white normal, yellow under the pointer.
    private func rows(_ w: Int, _ h: Int) -> Data {
        let row = (w * 3 + 3) & ~3
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(54 + row * h)); put(0); put(54)
        put(40); put(UInt32(w)); put(UInt32(h))
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(UInt32(row * h)); put(0); put(0); put(0); put(0)
        let colours: [[UInt8]] = [[255, 255, 255], [0, 255, 255], [0, 0, 255], [255, 0, 255], [128, 128, 128]]
        // Bottom row first: the sheet's last cell row is written first.
        for y in (0..<h).reversed() {
            var line = Data()
            for _ in 0..<w { line.append(contentsOf: colours[y * 5 / h]) }
            line.append(Data(repeating: 0, count: row - w * 3))
            d.append(line)
        }
        return d
    }

    // In the 4:3 frame the scene keeps its rows at the top, so a choice's option takes the pointer where it's drawn, as
    // on an iPad (set 1, Test Game 87): options 100 rows tall from y 260, the second from 360.
    @Test func aChoiceTakesThePointerWhereItsDrawnInTheSquareFrame() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("""
                UserDataDirectory\tvndegen\\squaretest
                FirstFlow\tflow\\t.txt
                MsgWinSquareFilename\tobj\\sq.bmp
                MsgWinSquarePosY\t720
                MsgWinSquareMovement\t240

                """.utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("sq.bmp", bmp(1280, 240, [255, 0, 0])), ("c.bmp", rows(400, 500))]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 3, next: 3, p: [0: 1, 1: 1], s: [1: "obj\\c.bmp"]),
                .init(op: 2, next: 0),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try start(root)
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_jump(e, "flow\\t.txt", 0))
        #expect(frame(e, view: (1024, 768)).h == 960)
        #expect(vnneko_debug_choice_count(e) == 2)
        // The pointer at the second option as drawn, 410 of the frame's 960 rows.
        vnneko_mouse_move(e, 0.5, 410.0 / 960)
        let f = frame(e, view: (1024, 768))
        #expect(pixel(f, 640, 410) == [255, 255, 0], "\(pixel(f, 640, 410))")
        #expect(pixel(f, 640, 300) == [255, 255, 255], "\(pixel(f, 640, 300))")

        // Focus down from the first option goes to the second where it's drawn, not a quarter lower.
        vnneko_mouse_move(e, 0.5, 330.0 / 960)
        #expect(vnneko_move_focus(e, 0, 1))
        let focused = frame(e, view: (1024, 768))
        #expect(pixel(focused, 640, 410) == [255, 255, 0], "\(pixel(focused, 640, 410))")
        #expect(pixel(focused, 640, 300) == [255, 255, 255], "\(pixel(focused, 640, 300))")
    }

    private func plateGame(_ folder: String, _ more: [String: Data] = [:]) throws -> URL {
        let rows: [[UInt8]] = [[200, 200, 200], [0, 200, 200], [0, 0, 200], [200, 0, 200], [80, 80, 80]]
        let sheet: Data = {
            let w = 50, h = 250
            var d = Data("BM".utf8)
            func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
            put(UInt32(54 + (w * 3 + 2) * h)); put(0); put(54)
            put(40); put(UInt32(w)); put(UInt32(h))
            d.append(contentsOf: [1, 0, 24, 0])
            put(0); put(UInt32((w * 3 + 2) * h)); put(0); put(0); put(0); put(0)
            for y in (0..<h).reversed() {
                for _ in 0..<w { d.append(contentsOf: rows[y / 50]) }
                d.append(contentsOf: [0, 0])
            }
            return d
        }()
        let under = ["#under Auto", "obj\\b.bmp", "2", "400,650", "1,5", "0,0", "0,1", "0,2", "0,3", "0,4", "none", "0,0", "1,1"]
        return try NekoSDKFlowFixture.game(more.merging([
            "cfg/init.ini": Data("""
                UserDataDirectory\tvndegen\\\(folder)
                MsgWinFilename\tobj\\win.bmp
                MsgWinPosY\t480
                MsgWinSquareFilename\tobj\\sq.bmp
                MsgWinSquarePosY\t720
                MsgWinSquareMovement\t240
                MsgWinButtonPlateFilename\tobj\\plate.bmp
                MsgWinButtonPlatePosX\t0
                MsgWinButtonPlatePosY\t626

                """.utf8),
            "cfg/msg_button.ini": Data((under.joined(separator: "\n") + "\n##\n").utf8),
            "obj.pak": NekoSDKFlowFixture.archive([
                ("win.bmp", bmp(1280, 241, [0, 0, 255])), ("sq.bmp", bmp(1280, 240, [255, 0, 0])),
                ("plate.bmp", bmp(1280, 94, [100, 100, 100])), ("b.bmp", sheet),
            ]),
        ]) { mine, _ in mine })
    }

    @Test func thePlateIsDrawnOnceOverAPartsScreenThatLeavesTheSceneShowing() throws {
        let root = try plateGame("partplatebandtest", ["scr/s.txt": NekoSDKFlowFixture.flow([.init(op: 0, next: 2), .init(op: 5, next: 0, s: [2: "hello"])])])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<20 { _ = vnneko_step(e, 16) }
        vnneko_mouse_move(e, 0.5, 0.3)
        for _ in 0..<5 where !String(cString: vnneko_debug_status(e)).contains("part screen") {
            vnneko_mouse_button(e, 0, true)
            vnneko_mouse_button(e, 0, false)
            for _ in 0..<20 { _ = vnneko_step(e, 16) }
        }
        #expect(String(cString: vnneko_debug_status(e)).contains("part screen"), "\(String(cString: vnneko_debug_status(e)))")
        _ = frame(e, view: (1024, 768))
        vnneko_mouse_move(e, 425.0 / 1280, 675.0 / 960)
        let square = frame(e, view: (1024, 768))
        #expect(square.h == 960)
        #expect(pixel(square, 10, 640) == [100, 100, 100])
        #expect(pixel(square, 10, 880) != [100, 100, 100])
        #expect(pixel(square, 10, 880) != pixel(square, 10, 721))
    }

    @Test func thePlateOverAPartsScreenTakesThePointerWhereItIsDrawn() throws {
        let root = try plateGame("partplatetest")
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<20 { _ = vnneko_step(e, 16) }
        #expect(String(cString: vnneko_debug_status(e)).contains("part screen"), "\(String(cString: vnneko_debug_status(e)))")
        vnneko_mouse_move(e, 10.0 / 1280, 300.0 / 960)
        var square = frame(e, view: (1024, 768))
        #expect(square.w == 1280 && square.h == 960)
        #expect(pixel(square, 10, 640) != [100, 100, 100])
        vnneko_mouse_move(e, 425.0 / 1280, 675.0 / 960)
        square = frame(e, view: (1024, 768))
        #expect(pixel(square, 10, 640) == [100, 100, 100])
        #expect(pixel(square, 425, 675) == [200, 200, 0])
        vnneko_mouse_button(e, 0, true)
        vnneko_mouse_button(e, 0, false)
        square = frame(e, view: (1024, 768))
        #expect(pixel(square, 425, 675) == [200, 0, 200])
        vnneko_mouse_move(e, 425.0 / 1280, 915.0 / 960)
        square = frame(e, view: (1024, 768))
        #expect(pixel(square, 425, 675) == [200, 0, 0])
    }

    @Test func theBandKeepsTheWindowsLayerUnderAPartsScreenThatLeavesTheSceneShowing() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("""
                UserDataDirectory\tvndegen\\partbandtest
                MsgWinFilename\tobj\\win.bmp
                MsgWinPosY\t480
                MsgWinSquareFilename\tobj\\sq.bmp
                MsgWinSquarePosY\t720
                MsgWinSquareMovement\t240

                """.utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("win.bmp", bmp(1280, 241, [0, 0, 255])), ("sq.bmp", bmp(1280, 240, [255, 0, 0]))]),
            "scr/s.txt": NekoSDKFlowFixture.flow([.init(op: 0, next: 2), .init(op: 5, next: 0, s: [2: "hello"])]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<20 { _ = vnneko_step(e, 16) }
        vnneko_mouse_move(e, 0.5, 0.3)
        for _ in 0..<5 where !String(cString: vnneko_debug_status(e)).contains("part screen") {
            vnneko_mouse_button(e, 0, true)
            vnneko_mouse_button(e, 0, false)
            for _ in 0..<20 { _ = vnneko_step(e, 16) }
        }
        #expect(String(cString: vnneko_debug_status(e)).contains("part screen"), "\(String(cString: vnneko_debug_status(e)))")
        let open = frame(e, view: (1024, 768))
        #expect(open.h == 960)
        #expect(pixel(open, 10, 955) != pixel(open, 10, 721))
        #expect(pixel(open, 805, 895) == [0, 255, 0])
        for _ in 0..<2 {
            vnneko_mouse_button(e, 0, true)
            vnneko_mouse_button(e, 0, false)
            for _ in 0..<3 { _ = vnneko_step(e, 16) }
        }
        #expect(vnneko_debug_variable(e, 1, 1) == 2)
        #expect(String(cString: vnneko_debug_status(e)).contains("part screen"), "\(String(cString: vnneko_debug_status(e)))")
        let covered = frame(e, view: (1024, 768))
        #expect(pixel(covered, 805, 895) == pixel(covered, 10, 955))
        #expect(pixel(covered, 10, 955) == pixel(open, 10, 955))
    }
}
