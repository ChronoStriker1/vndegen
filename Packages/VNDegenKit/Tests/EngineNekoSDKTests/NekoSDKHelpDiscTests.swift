import CNekoSDK
import Foundation
import Testing
import TestSupport

@Suite struct NekoSDKHelpDiscTests {
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

    private func steps(_ e: OpaquePointer, _ count: Int) {
        for _ in 0..<count { _ = vnneko_step(e, 10) }
    }

    @Test func aMenuHelpsDiscTurnsUnderItFromThePointersArrival() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\disctest\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([
                ("b.bmp", bmp(50, 250, [200, 200, 200])), ("h.bmp", bmp(10, 10, [0, 0, 250])), ("bar.bmp", bmp(100, 20, [0, 250, 0])),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        let green: [UInt8] = [0, 250, 0]
        steps(e, 5)
        #expect(pixel(e, 390, 310) != green)
        vnneko_mouse_move(e, 10.0 / 1280, 110.0 / 720)
        steps(e, 2)
        #expect(pixel(e, 390, 310) == green)
        #expect(pixel(e, 350, 270) != green)
        #expect(pixel(e, 305, 305) == [250, 0, 0])
        steps(e, 100)
        #expect(pixel(e, 390, 310) != green)
        #expect(pixel(e, 350, 270) == green)
        vnneko_mouse_move(e, 0.9, 0.9)
        steps(e, 10)
        vnneko_mouse_move(e, 10.0 / 1280, 110.0 / 720)
        steps(e, 2)
        #expect(pixel(e, 390, 310) != green)
        #expect(pixel(e, 350, 270) == green)
        vnneko_mouse_move(e, 0.9, 0.9)
        steps(e, 30)
        #expect(pixel(e, 350, 270) != green)
        vnneko_mouse_move(e, 10.0 / 1280, 110.0 / 720)
        steps(e, 2)
        #expect(pixel(e, 390, 310) == green)
    }

    @Test func aWindowButtonsCircleTurnsInPlaceOfTheWindowsPicture() throws {
        let entry = ["obj\\b.bmp", "1", "900,650", "1,5", "0,0", "0,1", "0,2", "0,3", "0,4",
                     "obj\\e.bmp", "1000,500", "1,1", "0,0", "1", "1", "2000",
                     "obj\\t.bmp", "1100,500", "1,1", "0,0", "1"]
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\twobytetest\nFirstFlow\tflow\\t.txt\n".utf8),
            "cfg/msg_button.ini": Data(("#inner Config\n" + entry.joined(separator: "\n") + "\n##\n").utf8),
            "obj.pak": NekoSDKFlowFixture.archive([
                ("b.bmp", bmp(50, 250, [200, 200, 200])), ("e.bmp", bmp(20, 20, [0, 200, 0])), ("t.bmp", bmp(10, 10, [0, 0, 250])),
            ]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([.init(op: 0, next: 2), .init(op: 5, next: 0, s: [2: "x"])]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        let green: [UInt8] = [0, 200, 0], red: [UInt8] = [250, 0, 0]
        steps(e, 30)
        #expect(pixel(e, 1002, 510) == green)
        #expect(pixel(e, 1105, 505) != red)
        vnneko_mouse_move(e, 910.0 / 1280, 660.0 / 720)
        steps(e, 2)
        #expect(pixel(e, 1002, 510) == green)
        #expect(pixel(e, 1105, 505) == red)
        steps(e, 73)
        #expect(pixel(e, 1002, 510) != green)
        #expect(pixel(e, 1105, 505) == red)
        vnneko_mouse_move(e, 0.1, 0.1)
        steps(e, 5)
        #expect(pixel(e, 1002, 510) == green)
        #expect(pixel(e, 1105, 505) != red)
    }

    @Test func aPlateAsWideAsTheScreenRisesFromItsRestAndHolds() throws {
        let under = ["#under Auto", "obj\\b.bmp", "2", "400,650", "1,5", "0,0", "0,1", "0,2", "0,3", "0,4", "none", "0,0", "1,1"]
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("""
                UserDataDirectory\tvndegen\\plateholdstest
                FirstFlow\tflow\\t.txt
                MsgWinButtonShowType\t3
                MsgWinButtonPlateFilename\tobj\\plate.bmp
                MsgWinButtonPlatePosX\t0
                MsgWinButtonPlatePosY\t626
                MsgWinButtonPlateBottomPosX\t0
                MsgWinButtonPlateBottomPosY\t700

                """.utf8),
            "cfg/msg_button.ini": Data((under.joined(separator: "\n") + "\n##\n").utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("b.bmp", bmp(50, 250, [200, 200, 200])), ("plate.bmp", bmp(1280, 94, [100, 100, 100]))]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([.init(op: 0, next: 2), .init(op: 5, next: 0, s: [2: "x"])]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        let grey: [UInt8] = [100, 100, 100]
        func at(_ y: Int) -> [UInt8] {
            vnneko_mouse_move(e, 10.0 / 1280, Float(y) / 720)
            steps(e, 3)
            return pixel(e, 10, 640)
        }
        steps(e, 30)
        #expect(at(300) != grey)
        #expect(pixel(e, 10, 710) == grey)
        #expect(at(650) != grey)
        #expect(at(705) == grey)
        #expect(at(650) == grey)
        #expect(at(600) != grey)
        #expect(at(650) != grey)
    }
}
