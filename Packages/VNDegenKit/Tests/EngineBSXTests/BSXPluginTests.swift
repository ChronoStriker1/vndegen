import CBSX
import Foundation
import Testing
import TestSupport
import VNCore
@testable import EngineBSX

@Suite struct BSXLayoutTests {
    private func probe(_ entries: [String]) -> FolderProbe { FolderProbe(root: URL(fileURLWithPath: "/game"), entries: Set(entries)) }

    @Test func findsTheScriptBesideTheArchives() {
        let layout = BSXLayout(probe(["mkiedl.exe", "boot.exe", "bsxx.dat", "bsxx.cfg", "Graphics.bsa", "bgm.bsa"]))
        #expect(layout?.root == "")
        #expect(layout?.evidence == "bsxx.dat")
    }

    @Test func findsAGameOneFolderDownIgnoringCase() {
        let layout = BSXLayout(probe(["MK2IEDL/BSXX.DAT", "MK2IEDL/bsxx.CFG", "MK2IEDL/GraphicsBU.BSA"]))
        #expect(layout?.root == "MK2IEDL")
    }

    @Test func wantsTheConfigAndPictures() {
        #expect(BSXLayout(probe(["bsxx.dat", "Graphics.bsa"])) == nil)
        #expect(BSXLayout(probe(["bsxx.dat", "bsxx.cfg"])) == nil)
        #expect(BSXLayout(probe(["a/b/bsxx.dat", "a/b/bsxx.cfg", "a/b/Graphics.bsa"])) == nil)
    }
}

@Suite struct BSXSaveTests {
    @Test func recognizesTheEngineSaveFiles() {
        #expect(BSXPlugin.saveName("C:\\Games\\MKIEDL\\save\\no015") == "no015")
        #expect(BSXPlugin.saveName("/x/save/no000") == "no000")
        #expect(BSXPlugin.saveName("/x/save/no-01") == "no-01")
        #expect(BSXPlugin.saveName("/x/save/common.dat") == "common.dat")
        #expect(BSXPlugin.saveName("/x/save/vndegen.cfg") == "vndegen.cfg")
        #expect(BSXPlugin.saveName("/x/save/no15") == nil)
        #expect(BSXPlugin.saveName("/x/save/no0151") == nil)
        #expect(BSXPlugin.saveName("/x/save/notes.txt") == nil)
        #expect(BSXPlugin.saveName("/x/bsxx.dat") == nil)
    }
}

@Suite struct BSXRuntimeTests {
    private func folder(_ name: String) throws -> URL {
        let root = TestFolders.root.appending(path: "\(name)-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        return root
    }

    @Test func probeReadsTheTitleFromBsxxIni() throws {
        let root = try folder("bsx-probe")
        var script = Data("BSXScript 3.0".utf8)
        script.append(Data(count: 0xa8))
        try script.write(to: root.appending(path: "bsxx.dat"))
        var ini = Data()
        for field in ["TEST@BISHOP", "Test Teacher", "Taro"] {
            ini.append(field.data(using: .utf16LittleEndian)!)
            ini.append(Data([0, 0]))
        }
        try ini.write(to: root.appending(path: "bsxx.ini"))
        var title = [CChar](repeating: 0, count: 256)
        #expect(vnbsx_probe(root.path, &title, title.count))
        #expect(String(decoding: title.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self) == "Test Teacher")
    }

    @Test func probeRefusesAnotherScript() throws {
        let root = try folder("bsx-other")
        try Data("BSXScript 2.0 and more".utf8).write(to: root.appending(path: "bsxx.dat"))
        var title = [CChar](repeating: 0, count: 256)
        #expect(!vnbsx_probe(root.path, &title, title.count))
    }
}

private struct BSXScriptBuilder {
    private(set) var code = Data()

    mutating func emit(_ bytes: [UInt8]) { code.append(contentsOf: bytes) }

    mutating func emit(_ value: Int32) { emit(Self.word(value)) }

    mutating func patch(at offset: Int, _ value: Int32) { code.replaceSubrange(offset ..< offset + 4, with: Self.word(value)) }

    static func word(_ value: Int32) -> [UInt8] { withUnsafeBytes(of: value.littleEndian) { Array($0) } }

    static func leaf(_ type: UInt8, _ value: Int32) -> [UInt8] { [type, 1] + word(value) }

    mutating func set(arg: Int32, _ value: [UInt8]) { emit([0x12, 0] + Self.word(0) + [4, 1] + Self.word(arg) + value) }

    mutating func choice(id: Int32, value: Int32, message: Int32) {
        set(arg: 1, Self.leaf(0, -2))
        set(arg: 2, Self.leaf(0, value))
        emit([0x33] + Self.word(id) + Self.word(value) + Self.word(message))
    }

    static func pool(_ strings: [String]) -> (offsets: Data, text: Data) {
        var offsets = Data(), text = Data()
        for string in strings {
            offsets.append(contentsOf: word(Int32(text.count / 2)))
            text.append(string.data(using: .utf16LittleEndian)!)
            text.append(contentsOf: [0, 0])
        }
        return (offsets, text)
    }

    func file(variables: [String], strings: [String], messages: [String], systemVariables: [(Int32, Int32)]) -> Data {
        let labels = Self.pool(["boot"]), strs = Self.pool(strings), vars = Self.pool(variables), msgs = Self.pool(messages)
        let sections = [Data(Self.word(0) + Self.word(0)), labels.text, strs.offsets, strs.text, Data(), Data(), vars.offsets,
                        vars.text, Data(), Data(), Data(), Data(), msgs.offsets, msgs.text]
        var header = Data("BSXScript 3.1".utf8)
        header.append(Data(count: 3))
        for value: Int32 in [0, 0, 2, 0, 0, 0] { header.append(contentsOf: Self.word(value)) }
        let codeOffset = 168 + 8 * systemVariables.count
        for value in [systemVariables.count, codeOffset, code.count, messages.count] { header.append(contentsOf: Self.word(Int32(value))) }
        var body = code, offset = codeOffset + code.count
        for section in sections {
            header.append(contentsOf: Self.word(Int32(offset)) + Self.word(Int32(section.count)))
            body.append(section)
            offset += section.count
        }
        for (id, variable) in systemVariables { header.append(contentsOf: Self.word(id) + Self.word(variable)) }
        return header + body
    }
}

@Suite struct BSXChoiceTests {
    private func game(pickedValue: Int32) throws -> UInt32 {
        let root = TestFolders.root.appending(path: "bsx-choice-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        var script = BSXScriptBuilder()
        script.choice(id: 0, value: 1, message: 0)
        script.choice(id: 1, value: 2, message: 1)
        script.emit([0x34])
        script.emit(-1)
        script.emit([0x11, 5] + BSXScriptBuilder.word(0) + BSXScriptBuilder.leaf(3, 4) + BSXScriptBuilder.leaf(0, 2))
        let jump = script.code.count
        script.emit([0x06])
        script.emit(0)
        script.set(arg: 1, BSXScriptBuilder.leaf(1, 0))
        script.set(arg: 2, BSXScriptBuilder.leaf(0, 1))
        script.emit([0x1e])
        script.set(arg: 1, BSXScriptBuilder.leaf(0, 0))
        script.set(arg: 2, BSXScriptBuilder.leaf(0, 0))
        script.emit([0x27])
        script.patch(at: jump + 1, Int32(script.code.count))
        script.emit([0x1d, 0])
        script.emit(2)
        let file = script.file(variables: ["#day", "#time", "#result", "#select_value", "#select_type"], strings: ["white"],
                               messages: ["One", "Two", "Done"], systemVariables: [(0x103_0000, 0), (0x103_000a, 3), (0x103_03e8, 4)])
        try file.write(to: root.appending(path: "bsxx.dat"))
        try Data("[main window]\nwidth = 800\nheight = 600\n[select window]\nvertical datum line = 300\n".utf8)
            .write(to: root.appending(path: "bsxx.cfg"))
        var archive = Data("BSArc".utf8) + Data(count: 3)
        archive.append(contentsOf: [2, 0, 1, 0] + BSXScriptBuilder.word(17) + [0x78] + BSXScriptBuilder.word(0) + BSXScriptBuilder.word(16)
                       + BSXScriptBuilder.word(1) + Array("a.bsg".utf8) + [0])
        try archive.write(to: root.appending(path: "Graphics.bsa"))
        var error = [CChar](repeating: 0, count: 256)
        let bx = try #require(root.path.withCString { dir in
            var config = VNBSXConfig(gameDir: dir, saveDir: dir, logPath: nil, audioRate: 44100, startTime: 1)
            return vnbsx_open(&config, &error, error.count)
        })
        defer { vnbsx_close(bx) }
        for _ in 0 ..< 30 { #expect(vnbsx_frame(bx, 16)) }
        vnbsx_mouse_move(bx, 400, pickedValue == 1 ? 284 : 316)
        #expect(vnbsx_frame(bx, 16))
        vnbsx_mouse_button(bx, Int32(VNBSX_MOUSE_LEFT), true)
        #expect(vnbsx_frame(bx, 16))
        vnbsx_mouse_button(bx, Int32(VNBSX_MOUSE_LEFT), false)
        for _ in 0 ..< 30 { #expect(vnbsx_frame(bx, 16)) }
        var width: Int32 = 0, height: Int32 = 0
        let pixels = try #require(vnbsx_pixels(bx, &width, &height))
        return pixels[10 * Int(width) + 10]
    }

    @Test func aValueChoiceStoresItsValueAndGoesOn() throws {
        #expect(try game(pickedValue: 2) == 0xffff_ffff)
        #expect(try game(pickedValue: 1) != 0xffff_ffff)
    }
}
