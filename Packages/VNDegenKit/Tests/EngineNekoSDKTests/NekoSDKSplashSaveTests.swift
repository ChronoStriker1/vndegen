import CNekoSDK
import Foundation
import Testing
import TestSupport

// A splash emitter (command 200, Test Game 88) is a record of its own in a save, naming its droplet's picture with
// command 200's numbers after it, as the PC writes it. A load brings the emitter back, so the next save holds it again.
@Suite struct NekoSDKSplashSaveTests {
    // A 2 by 2 white 24-bit BMP standing in for the droplet.
    private func drop() -> Data {
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(54 + 16)); put(0); put(54)
        put(40); put(2); put(2)
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(16); put(0); put(0); put(0); put(0)
        for _ in 0..<2 { d.append(contentsOf: [255, 255, 255, 255, 255, 255, 0, 0]) }
        return d
    }

    private func int32(_ data: Data, _ at: Int) -> Int32 {
        data.subdata(in: (data.startIndex + at)..<(data.startIndex + at + 4)).withUnsafeBytes { $0.loadUnaligned(as: Int32.self) }
    }

    // The emitter's numbers after its picture's name in a save: parameters 0, 5, 1 to 4, 6, 10, 11 and 7 to 9.
    private func splashNumbers(_ save: Data, _ name: String) -> [Int32]? {
        guard let at = save.range(of: Data(name.utf8)) else { return nil }
        let after = at.upperBound - save.startIndex
        #expect(int32(save, at.lowerBound - save.startIndex - 4) == Int32(name.utf8.count))
        return (0..<12).map { int32(save, after + 4 * $0) }
    }

    private func saveFile(_ data: URL, _ slot: Int) throws -> Data {
        let folder = data.appendingPathComponent("AppData/Roaming/waffle/mother3/save")
        let name = String(format: "save%03d.sav", slot)
        let file = try #require(try FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: nil)
            .first { $0.lastPathComponent.lowercased() == name })
        return try NekoSDKFlowFixture.unpacked(Data(contentsOf: file))
    }

    @Test func aSplashEmitterIsSavedAndComesBack() throws {
        let picture = "parts\\drop.bmp"
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\twaffle\\mother3\nFirstFlow\tflow\\t.txt\n".utf8),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 200, next: 3, p: [0: 9500, 1: 640, 2: 360, 3: 1280, 4: 720, 5: 180, 6: 20, 7: 5, 8: 10, 9: 10, 10: 500, 11: 1500, 12: 300],
                      s: [1: picture]),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
            "parts.pak": NekoSDKFlowFixture.archive([("drop.bmp", drop())]),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        _ = vnneko_debug_press(first, "start")
        for _ in 0..<10 { _ = vnneko_step(first, 16) }
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)
        let expected: [Int32] = [9500, 180, 640, 360, 1280, 720, 20, 500, 1500, 5, 10, 10]
        let firstSave = try saveFile(data, 1)
        #expect(splashNumbers(firstSave, picture) == expected)

        // Loaded in a fresh run, whose story hasn't reached the emitter, and saved again: the emitter is there.
        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(second) }
        #expect(vnneko_debug_load(second, 1))
        for _ in 0..<3 { _ = vnneko_step(second, 16) }
        #expect(vnneko_debug_save(second, 2))
        let secondSave = try saveFile(data, 2)
        #expect(splashNumbers(secondSave, picture) == expected)
    }
}
