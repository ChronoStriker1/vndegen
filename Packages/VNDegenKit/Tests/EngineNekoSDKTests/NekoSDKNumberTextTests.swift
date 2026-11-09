import CNekoSDK
import Foundation
import Testing
import TestSupport

// Test Game 82's Config draws its numbers as text, from no picture: white with a purple edge, right-aligned, no leading
// zeros (`number NAME text RIGHT BASELINE SIZE STEP R G B OR OG OB`).
@Suite struct NekoSDKNumberTextTests {
    private func frame(_ e: OpaquePointer) -> (UnsafePointer<UInt8>, Int)? {
        var w: Int32 = 0, h: Int32 = 0, changed = false
        guard let px = vnneko_frame(e, &w, &h, &changed) else { return nil }
        return (px, Int(w))
    }

    @Test func aNumberIsWhiteTextWithAnEdgeEndingAtItsRight() throws {
        let root = try NekoSDKFlowFixture.game(["cfg/init.ini": Data("UserDataDirectory\tvndegen\\cardtest\n".utf8)])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_press(e, "config"))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        let (px, w) = try #require(frame(e))
        // Opacity starts at 60: two digits in the 30 pixels left of x 200, above the baseline at 100.
        var white = 0, edge = 0, outside = 0
        for y in 70..<104 {
            for x in 150..<230 {
                let at = (y * w + x) * 4, rgb = (px[at], px[at + 1], px[at + 2])
                let lit = rgb.0 > 230 && rgb.1 > 230 && rgb.2 > 230, purple = rgb.0 > 60 && rgb.1 < 30 && rgb.2 > 60
                if (lit || purple) && (x < 168 || x > 202) { outside += 1 }
                if lit { white += 1 }
                if purple { edge += 1 }
            }
        }
        #expect(white > 40 && edge > 40, "white \(white), edge \(edge)")
        #expect(outside == 0, "\(outside) pixels outside the two digits' 30")
    }
}
