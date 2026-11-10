import CNekoSDK
import Foundation
import Testing
import TestSupport

@Suite struct NekoSDKTextAdvanceTests {
    private func engine(_ folder: String) throws -> (OpaquePointer, URL) {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\\(folder)\nFirstFlow\tflow\\t.txt\n".utf8),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([.init(op: 0, next: 2), .init(op: 5, next: 0, s: [2: "x"])]),
        ])
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        return (e, root)
    }

    @Test func aTwoByteCharacterAdvancesByItsTwoBytesWidths() throws {
        let (keyed, keyedRoot) = try engine("twobytetest")
        defer { vnneko_stop(keyed); try? FileManager.default.removeItem(at: keyedRoot) }
        let (plain, plainRoot) = try engine("gttest")
        defer { vnneko_stop(plain); try? FileManager.default.removeItem(at: plainRoot) }
        let missing = vnneko_debug_text_advance(keyed, 0)
        #expect(missing > 0)
        func width(_ c: Character) -> Int32 { vnneko_debug_text_advance(keyed, c.unicodeScalars.first!.value) }
        #expect(width("\u{201C}") == missing + width("g"))
        #expect(width("\u{201D}") == missing + width("h"))
        #expect(width("\u{2018}") == missing + width("e"))
        #expect(width("\u{2015}") == missing + width("\\"))
        #expect(width("\u{266A}") == missing + width("\u{F4}"))
        #expect(width("\u{3042}") == width("\u{201A}") + width("\u{A0}"))
        for c in ["A", "g", "\\", "\u{F4}"] as [Character] {
            #expect(width(c) == vnneko_debug_text_advance(plain, c.unicodeScalars.first!.value))
        }
    }

    @Test func aWindowPictureShowsWithTheWindow() throws {
        func bmp(_ w: Int, _ h: Int, _ bgr: [UInt8]) -> Data {
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
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\twobytetest\nFirstFlow\tflow\\t.txt\n".utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("e.bmp", bmp(20, 20, [0, 200, 0]))]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 2, next: 3, p: [0: 500]),
                .init(op: 5, next: 0, s: [2: "x"]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        func emblem() -> Bool {
            var w: Int32 = 0, h: Int32 = 0, changed = false
            guard let px = vnneko_frame(e, &w, &h, &changed) else { return false }
            let at = (510 * Int(w) + 1010) * 4
            return px[at] == 0 && px[at + 1] >= 100 && px[at + 2] == 0
        }
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        #expect(!emblem())
        for _ in 0..<60 { _ = vnneko_step(e, 16) }
        #expect(emblem())
    }
}
