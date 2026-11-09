import CNekoSDK
import Foundation
import Testing
import TestSupport

// A screen's `band` multiplies the rows between its lines by the colour between them, over all the screen draws, as Test
// Game 88's Config darkens its bottom (core/tables/bandtest.inc).
@Suite struct NekoSDKBandTests {
    // A white 24-bit BMP the size of the screen.
    private func white() -> Data {
        let w = 1280, h = 720, row = w * 3
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(54 + row * h)); put(0); put(54)
        put(40); put(UInt32(w)); put(UInt32(h))
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(UInt32(row * h)); put(0); put(0); put(0); put(0)
        d.append(Data(repeating: 255, count: row * h))
        return d
    }

    @Test func rowsBetweenTheLinesAreMultiplied() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\bandtest\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("w.bmp", white())]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        var w: Int32 = 0, h: Int32 = 0, changed = false
        let px = try #require(vnneko_frame(e, &w, &h, &changed))
        func pixel(_ x: Int, _ y: Int) -> [Int] { let at = (y * Int(w) + x) * 4; return [Int(px[at]), Int(px[at + 1]), Int(px[at + 2])] }
        #expect(pixel(600, 9) == [255, 255, 255])
        #expect(pixel(600, 10) == [255, 255, 255])
        // A fifth of the way: 255 less a fifth of 127, 191 and 223.
        #expect(pixel(600, 12) == [230, 217, 210])
        #expect(pixel(0, 20) == [128, 64, 32])
        #expect(pixel(1279, 20) == [128, 64, 32])
        #expect(pixel(600, 21) == [255, 255, 255])
    }
}
