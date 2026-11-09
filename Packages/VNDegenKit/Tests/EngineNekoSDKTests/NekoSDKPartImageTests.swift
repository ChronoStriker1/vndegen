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
}
