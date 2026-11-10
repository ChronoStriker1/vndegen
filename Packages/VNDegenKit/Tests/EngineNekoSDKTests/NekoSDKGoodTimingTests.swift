import CNekoSDK
import Foundation
import Testing
import TestSupport

@Suite struct NekoSDKGoodTimingTests {
    private func sheet() -> Data {
        let w = 40, h = 100, row = w * 3
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(54 + row * h)); put(0); put(54)
        put(40); put(UInt32(w)); put(UInt32(h))
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(UInt32(row * h)); put(0); put(0); put(0); put(0)
        for _ in 0..<(w * h) { d.append(contentsOf: [255, 0, 255]) }
        return d
    }

    private func config(_ scene: String) -> Data {
        var text = "#Extension\ngt\\scr\\\(scene)x.txt\n##\n"
        for group in ["A", "B", "C"] {
            text += "#Group\(group)\ngt\\scr\\\(scene)\(group)1.txt\tgt\\scr\\\(scene)\(group)2.txt\n##\n"
        }
        return Data((text + "#Ejaculate\ngt\\scr\\\(scene)e.txt\n##\n").utf8)
    }

    private func game() throws -> URL {
        func button(_ name: String, _ x: Int) -> String {
            (["#inner \(name)", "obj\\b.bmp", name == "Ejaculation" ? "0" : "2", "\(x),600", "1,5", "0,0", "0,1", "0,2", "0,3", "0,4", "none", "0,0", "1,1"]).joined(separator: "\n") + "\n##\n"
        }
        let line = NekoSDKFlowFixture.flow([.init(op: 0, next: 2), .init(op: 5, next: 0, s: [2: "x"])])
        func scene(_ name: String) -> Data {
            NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 153, next: 3, s: [1: "gt\\cfg\\\(name).txt"]),
                .init(op: 5, next: 4, s: [2: "before"]),
                .init(op: 154, next: 5),
                .init(op: 5, next: 0, s: [2: "after"]),
            ])
        }
        var files: [String: Data] = [
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\gttest\nFirstFlow\tflow\\t.txt\n".utf8),
            "cfg/msg_button.ini": Data((button("Extension", 1000) + button("Ejaculation", 900) + button("GOODTiming", 800)).utf8),
            "obj.pak": NekoSDKFlowFixture.archive([("b.bmp", sheet())]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s1.txt"]),
                .init(op: 1, next: 4, s: [1: "scr\\s2.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s1.txt": scene("one"),
            "scr/s2.txt": scene("two"),
            "gt/cfg/one.txt": config("one"),
            "gt/cfg/two.txt": config("two"),
        ]
        for name in ["one", "two"] {
            for part in ["x", "e", "A1", "A2", "B1", "B2", "C1", "C2"] { files["gt/scr/\(name)\(part).txt"] = line }
        }
        return try NekoSDKFlowFixture.game(files)
    }

    private func playing(_ e: OpaquePointer) -> String {
        for _ in 0..<40 { _ = vnneko_step(e, 16) }
        return String(String(cString: vnneko_debug_status(e)).split(separator: " ").first ?? "")
    }

    private func click(_ e: OpaquePointer) {
        vnneko_mouse_move(e, 0.5, 0.3)
        vnneko_mouse_button(e, 0, true)
        vnneko_mouse_button(e, 0, false)
    }

    @Test func extendedScenesPlayTheirGroupsWholeNeverOneTwiceRunning() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        #expect(playing(e) == "scr\\s1.txt")
        #expect(vnneko_debug_press(e, "Extension"))
        click(e)
        #expect(playing(e) == "gt\\scr\\onex.txt")
        var groups = ""
        for _ in 0..<40 {
            click(e)
            let first = playing(e)
            click(e)
            let second = playing(e)
            let group = String(first.dropLast(5).last ?? "?")
            #expect(first == "gt\\scr\\one\(group)1.txt" && second == "gt\\scr\\one\(group)2.txt", "\(first) then \(second)")
            groups += group
        }
        #expect(groups.first == "A")
        #expect(!zip(groups, groups.dropFirst()).contains { $0 == $1 }, "\(groups)")
        #expect(Set(groups) == ["A", "B", "C"])
    }

    @Test func extendStaysOnForTheNextSceneAndTheNextRun() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        #expect(playing(e) == "scr\\s1.txt")
        #expect(vnneko_debug_press(e, "Extension"))
        click(e)
        #expect(playing(e) == "gt\\scr\\onex.txt")
        #expect(vnneko_debug_press(e, "Ejaculation"))
        #expect(playing(e) == "scr\\s1.txt")
        click(e)
        #expect(playing(e) == "scr\\s2.txt")
        click(e)
        #expect(playing(e) == "gt\\scr\\twox.txt")
        vnneko_stop(e)

        let again = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(again) }
        #expect(playing(again) == "scr\\s1.txt")
        click(again)
        #expect(playing(again) == "gt\\scr\\onex.txt")
    }

    @Test func extendDoesNothingWhileTheSwitchIsOff() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        #expect(playing(e) == "scr\\s1.txt")
        #expect(vnneko_debug_press(e, "Extension"))
        #expect(vnneko_debug_press(e, "GOODTiming"))
        click(e)
        #expect(playing(e) == "scr\\s1.txt")
        click(e)
        #expect(playing(e) == "scr\\s2.txt")
    }

    @Test func theSwitchTurnedOffEndsAnExtensionAtItsNextScript() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let e = try #require(NekoSDKFlowFixture.start(root, data: root.appendingPathComponent("data")))
        defer { vnneko_stop(e) }
        #expect(playing(e) == "scr\\s1.txt")
        #expect(vnneko_debug_press(e, "Extension"))
        click(e)
        #expect(playing(e) == "gt\\scr\\onex.txt")
        click(e)
        #expect(playing(e) == "gt\\scr\\oneA1.txt")
        #expect(vnneko_debug_press(e, "GOODTiming"))
        click(e)
        #expect(playing(e) == "scr\\s1.txt")
        click(e)
        #expect(playing(e) == "scr\\s2.txt")
    }
}
