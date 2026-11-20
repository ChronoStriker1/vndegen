import CNekoSDK
import Foundation
import Testing
import TestSupport

@Suite struct NekoSDKEdensRitterTests {
    private let line = "Though the orcs were twice their number, \nthe fighters came through."

    private func game(_ folder: String = "Eden's Ritter - Paladins of Ecstasy") throws -> URL {
        let ini = """
        UserDataDirectory,waffle\\\(folder),// folder\r
        FirstFlow,flow\\t.txt,// first\r
        ExFlow,flow\\x.txt,// extra\r

        """
        return try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data(ini.utf8),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "flow/x.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\e.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 5, next: 3, s: [2: line]),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
            "scr/e.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 5, next: 3, s: [2: "extra"]),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
    }

    private func status(_ e: OpaquePointer) -> String { String(cString: vnneko_debug_status(e)) }

    @Test func aTitleButtonStartsTheFlowInitIniNamesByCommas() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        for _ in 0..<5 { _ = vnneko_step(e, 16) }
        #expect(status(e).contains("menu title"), "\(status(e))")
        #expect(vnneko_debug_press(e, "flowkey:ExFlow"))
        for _ in 0..<30 { _ = vnneko_step(e, 16) }
        #expect(status(e).hasPrefix("scr\\e.txt"), "\(status(e))")
    }

    @Test(arguments: ["Eden's Ritter - Paladins of Ecstasy", "Eden's Ritter - Priestess of Pleasure"])
    func aSaveCarriesTheLineAsItsCommentAndLoads(name: String) throws {
        let root = try game(name)
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(e) }
        #expect(vnneko_debug_press(e, "start"))
        for _ in 0..<120 { _ = vnneko_step(e, 16) }
        #expect(status(e).hasPrefix("scr\\s.txt #3 click"), "\(status(e))")
        #expect(vnneko_debug_save(e, 1))
        let folder = data.appendingPathComponent("AppData/Roaming/waffle/\(name)/save")
        let plain = try NekoSDKFlowFixture.unpacked(Data(contentsOf: folder.appendingPathComponent("save001.sav")))
        let length = Int(plain.subdata(in: 1..<5).withUnsafeBytes { $0.loadUnaligned(as: Int32.self) })
        #expect(String(data: plain.subdata(in: 5..<(5 + length)), encoding: .shiftJIS) == line)
        #expect(vnneko_debug_load(e, 1))
        #expect(status(e).contains("scr\\s.txt #3"), "\(status(e))")
    }
}
