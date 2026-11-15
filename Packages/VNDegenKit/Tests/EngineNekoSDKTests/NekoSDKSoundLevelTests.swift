import CNekoSDK
import Foundation
import Testing
import TestSupport

@Suite struct NekoSDKSoundLevelTests {
    private func steadyWave(seconds: Int) -> Data {
        let frames = 44100 * seconds
        var d = Data("RIFF".utf8)
        func put32(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        func put16(_ v: UInt16) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put32(UInt32(36 + frames * 4))
        d.append(Data("WAVEfmt ".utf8))
        put32(16); put16(1); put16(2); put32(44100); put32(44100 * 4); put16(4); put16(16)
        d.append(Data("data".utf8))
        put32(UInt32(frames * 4))
        for _ in 0..<(frames * 2) { put16(UInt16(16384)) }
        return d
    }

    private func levels(_ e: OpaquePointer) -> (left: Float, right: Float) {
        var out = [Float](repeating: 0, count: 2 * 256)
        vnneko_audio_render(e, &out, 256)
        return (out[200], out[201])
    }

    @Test func volumeAndPanFollowThePCsLevelLaw() throws {
        let sound = "se\\t.wav"
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("FirstFlow\tflow\\t.txt\n".utf8),
            "se.pak": NekoSDKFlowFixture.archive([("t.wav", steadyWave(seconds: 2))]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 35, next: 3, p: [1: 100, 2: 50, 4: 1], s: [1: sound]),
                .init(op: 2, next: 4, p: [0: 500]),
                .init(op: 36, next: 5),
                .init(op: 35, next: 6, p: [1: 50, 4: 1], s: [1: sound]),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_jump(e, "flow\\t.txt", 0))
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        let source: Float = 0.5, group = pow(Float(0.8), 1.661), half = pow(Float(0.5), 1.661)
        let panned = levels(e)
        #expect(abs(panned.left - source * group * half) < 0.005, "\(panned)")
        #expect(abs(panned.right - source * group) < 0.005, "\(panned)")
        for _ in 0..<40 { _ = vnneko_step(e, 16) }
        let quiet = levels(e)
        #expect(abs(quiet.left - source * group * half) < 0.005, "\(quiet)")
        #expect(abs(quiet.right - quiet.left) < 0.001, "\(quiet)")
    }
}
