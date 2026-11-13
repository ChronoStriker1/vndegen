import CNekoSDK
import Foundation
import Testing
import TestSupport

@Suite struct NekoSDKDamagedDataTests {
    private func bmp(_ w: Int, _ h: Int) -> Data {
        let row = (w * 3 + 3) & ~3
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(54 + row * h)); put(0); put(54)
        put(40); put(UInt32(w)); put(UInt32(h))
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(UInt32(row * h)); put(0); put(0); put(0); put(0)
        d.append(Data(repeating: 120, count: row * h))
        return d
    }

    private func scene(_ nodes: [NekoSDKFlowFixture.Node]) throws -> URL {
        try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("FirstFlow\tflow\\t.txt\n".utf8),
            "bg.pak": NekoSDKFlowFixture.archive([("a.bmp", bmp(64, 48))]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\a.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/a.txt": NekoSDKFlowFixture.flow(nodes),
        ])
    }

    @Test func aHugeBlurFinishesItsFrame() throws {
        let root = try scene([
            .init(op: 0, next: 2),
            .init(op: 15, next: 3, p: [3: 10], s: [1: "bg\\a.bmp"]),
            .init(op: 18, next: 4, p: [3: 10, 20: 400_000_000, 21: 1]),
            .init(op: 9, next: 5),
            .init(op: 5, next: 0, s: [2: "Hello"]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        let started = ContinuousClock.now
        for _ in 0..<30 where !vnneko_debug_waiting_for_click(e) {
            _ = vnneko_step(e, 16)
            var w: Int32 = 0, h: Int32 = 0, changed = false
            _ = vnneko_frame(e, &w, &h, &changed)
        }
        #expect(vnneko_debug_waiting_for_click(e))
        #expect(ContinuousClock.now - started < .seconds(10))
    }

    @Test func aLoopThroughAFadeYieldsEachStep() throws {
        let root = try scene([
            .init(op: 0, next: 2),
            .init(op: 15, next: 3, p: [3: 10], s: [1: "bg\\a.bmp"]),
            .init(op: 9, next: 2, p: [1: 500]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        _ = vnneko_step(e, 16)
        let started = ContinuousClock.now
        _ = vnneko_step(e, 16)
        #expect(ContinuousClock.now - started < .seconds(2))
    }
}

extension NekoSDKDamagedDataTests {
    @Test func anUnreadableCfgSavIsKeptAside() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\damaged\nFirstFlow\tflow\\t.txt\n".utf8),
            "flow/t.txt": NekoSDKFlowFixture.flow([.init(op: 0, next: 2), .init(op: 2, next: 0)]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let save = root.appendingPathComponent("data/AppData/Roaming/vndegen/damaged/save")
        try FileManager.default.createDirectory(at: save, withIntermediateDirectories: true)
        let whole = NekoSDKFlowFixture.packed(Data(count: 48000))
        let cut = whole.prefix(whole.count / 2)
        try cut.write(to: save.appendingPathComponent("cfg.sav"))
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        vnneko_stop(e)
        #expect(try Data(contentsOf: save.appendingPathComponent("cfg.sav.unreadable")) == cut)
        #expect((try? Data(contentsOf: save.appendingPathComponent("cfg.sav"))) != cut)
    }
}
