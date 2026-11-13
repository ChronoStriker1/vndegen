import Foundation
import Testing
@testable import EngineAliceSoft

@Suite struct System4SaveRuleTests {
    private func game(ini: Data) throws -> URL {
        let root = FileManager.default.temporaryDirectory.appending(path: "system4-saves-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        try ini.write(to: root.appending(path: "AliceStart.ini"))
        return root
    }

    @Test func readsAShiftJISSaveFolderWhateverTheKeysCase() throws {
        let cp932: [UInt8] = [0x83, 0x5A, 0x81, 0x5B, 0x83, 0x75]
        let root = try game(ini: Data("CodeName = \"Game.ain\"\r\nsavefolder = \"".utf8) + Data(cp932) + Data("\"\r\n".utf8))
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(System4Session.saveFolderName(root) == "セーブ")
    }

    @Test func keepsAUTF8SaveFolder() throws {
        let root = try game(ini: Data("SaveFolder=Save Data\n".utf8))
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(System4Session.saveFolderName(root) == "Save Data")
    }

    @Test func dropsBackupPathsThatLeaveTheSaveFolder() throws {
        let root = try game(ini: Data("SaveFolder=SaveData\n".utf8))
        defer { try? FileManager.default.removeItem(at: root) }
        let rule = try #require(System4Session.saveLocations(root: root, dataDirectory: root).first).relativePath
        #expect(rule("C:/Games/Game/SaveData/00.asd") == "00.asd")
        #expect(rule("C:/Games/Game/savedata/sub/01.asd") == "sub/01.asd")
        #expect(rule("C:/Games/Game/SaveData/../../x.asd") == nil)
        #expect(rule("C:/Games/Game/SaveData/./x.asd") == nil)
        #expect(rule("C:/Games/Game/Other/00.asd") == nil)
    }
}
