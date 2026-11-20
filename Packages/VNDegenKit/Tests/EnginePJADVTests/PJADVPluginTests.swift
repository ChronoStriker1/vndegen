import CPJADV
import Foundation
import Testing
import TestSupport
import VNCore
@testable import EnginePJADV

private func pac(_ files: [(String, Data)]) -> Data {
    var out = Data("GAMEDAT PAC2".utf8)
    func u32(_ v: Int) -> Data { withUnsafeBytes(of: UInt32(v).littleEndian) { Data($0) } }
    out.append(u32(files.count))
    for (name, _) in files {
        var field = Data(name.utf8)
        field.append(Data(count: 32 - field.count))
        out.append(field)
    }
    var offset = 0
    for (_, data) in files {
        out.append(u32(offset))
        out.append(u32(data.count))
        offset += data.count
    }
    for (_, data) in files { out.append(data) }
    return out
}

private func folder(_ name: String) throws -> URL {
    let root = TestFolders.root.appending(path: "\(name)-\(UUID().uuidString)")
    try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
    return root
}

private func game(in root: URL, title: String = "Test Cleaning! Version1.00") throws {
    var scenario = Data("PJADV_SF0001".utf8)
    scenario.append(Data(count: 20))
    scenario.append(Data([1, 0, 0, 1]))
    var text = Data("PJADV_TF0001".utf8)
    text.append(Data(count: 4))
    let files = [
        ("GAME.ini", Data("gametitle=\"\(title)\"\r\nicon=\"icon.ico\"\r\n".utf8)),
        ("scenario.dat", scenario),
        ("textdata.bin", text),
    ]
    try pac(files).write(to: root.appending(path: "archive.dat"))
}

@Suite struct PJADVLayoutTests {
    @Test func findsTheScenarioArchive() throws {
        let root = try folder("pjadv-layout")
        try game(in: root)
        try Data("MZ".utf8).write(to: root.appending(path: "Cleaner.exe"))
        let layout = PJADVLayout(FolderProbe(scanning: root))
        #expect(layout?.root == "")
        #expect(layout?.evidence == "archive.dat")
        #expect(PJADVPlugin().detect(FolderProbe(scanning: root))?.engine == .pjadv)
    }

    @Test func findsAGameOneFolderDown() throws {
        let root = try folder("pjadv-nested")
        let inner = root.appending(path: "Paradise Cleaning")
        try FileManager.default.createDirectory(at: inner, withIntermediateDirectories: true)
        try game(in: inner)
        #expect(PJADVLayout(FolderProbe(scanning: root))?.root == "Paradise Cleaning")
    }

    @Test func ignoresOtherArchives() throws {
        let root = try folder("pjadv-other")
        try pac([("BGM001.ogg", Data("OggS".utf8))]).write(to: root.appending(path: "archive.dat"))
        #expect(PJADVLayout(FolderProbe(scanning: root)) == nil)
        try Data("not an archive".utf8).write(to: root.appending(path: "archive.dat"))
        #expect(PJADVLayout(FolderProbe(scanning: root)) == nil)
    }

    @Test func probeReadsTheTitle() throws {
        let root = try folder("pjadv-probe")
        try game(in: root)
        var title = [CChar](repeating: 0, count: 128)
        #expect(vnpjadv_probe(root.path, &title, title.count))
        #expect(String(cString: title) == "Test Cleaning! Version1.00")
    }
}

@Suite struct PJADVSaveTests {
    @Test func recognizesTheEngineSaveFiles() {
        #expect(PJADVPlugin.saveName("C:\\Games\\Cleaning\\save\\syssave.dat") == "syssave.dat")
        #expect(PJADVPlugin.saveName("/x/save/syssvpo.dat") == "syssvpo.dat")
        #expect(PJADVPlugin.saveName("/x/save/SAVEDATA.DAT") == "SAVEDATA.DAT")
        #expect(PJADVPlugin.saveName("/x/save/pconfig.dat") == "pconfig.dat")
        #expect(PJADVPlugin.saveName("/x/graphic.dat") == nil)
        #expect(PJADVPlugin.saveName("/x/save/notes.txt") == nil)
    }
}

@Suite struct PJADVLanguageTests {
    @Test func followsTheDeviceLanguageAndDefaultsToEnglish() {
        #expect(PJADVLanguage.choose(preferred: ["ja-JP", "en-US"]) == .japanese)
        #expect(PJADVLanguage.choose(preferred: ["zh-Hans-CN"]) == .chinese)
        #expect(PJADVLanguage.choose(preferred: ["fr-FR", "en-GB"]) == .english)
        #expect(PJADVLanguage.choose(preferred: ["de-DE"]) == .english)
        #expect(PJADVLanguage.english.rawValue == 1)
        #expect(PJADVLanguage.japanese.rawValue == 2)
        #expect(PJADVLanguage.chinese.rawValue == 3)
    }
}
