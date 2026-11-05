import Foundation
import Testing
import VNCore
@testable import EngineUnity

@Suite struct UnityLayoutTests {
    private func probe(_ entries: [String]) -> FolderProbe { FolderProbe(root: URL(fileURLWithPath: "/game"), entries: Set(entries)) }

    /// `deeper` stands in for the disk below the probe's three levels.
    private func layout(_ entries: [String], deeper: [String: [String]] = [:]) -> UnityLayout? {
        UnityLayout(probe(entries)) { deeper[$0] ?? [] }
    }

    @Test func findsAMonoGameAtTheTop() {
        let found = layout(["Game.exe", "UnityPlayer.dll", "MonoBleedingEdge", "Game_Data", "Game_Data/globalgamemanagers",
                            "Game_Data/Managed", "Game_Data/Managed/Assembly-CSharp.dll"])
        #expect(found?.root == "")
        #expect(found?.evidence == "Game_Data/globalgamemanagers")
        #expect(found?.confidence == .likely)
        #expect(found?.backend == .mono)
        #expect(found?.macApp == nil)
    }

    @Test func findsAnIL2CPPGameOneFolderDown() {
        let found = layout(["Wrap", "Wrap/My Game.exe", "Wrap/GameAssembly.dll", "Wrap/UnityPlayer.dll", "Wrap/My Game_Data",
                            "Wrap/My Game_Data/data.unity3d", "Wrap/My Game_Data/il2cpp_data"])
        #expect(found?.root == "Wrap")
        #expect(found?.evidence == "Wrap/My Game_Data/data.unity3d")
        #expect(found?.backend == .il2cpp)
    }

    @Test func readsADataFolderBelowTheProbe() {
        let found = layout(["A", "A/B", "A/B/Old.exe", "A/B/Old_Data"], deeper: ["A/B/Old_Data": ["mainData", "Managed"]])
        #expect(found?.root == "A/B")
        #expect(found?.evidence == "A/B/Old_Data/mainData")
        #expect(found?.confidence == .likely)
        #expect(found?.backend == .mono)
    }

    @Test func isOnlyWeakWithoutThePlayer() {
        let found = layout(["Game_Data", "Game_Data/globalgamemanagers"])
        #expect(found?.confidence == .weak)
        #expect(found?.backend == nil)
    }

    @Test func wantsADataFileInTheDataFolder() {
        #expect(layout(["Game.exe", "Game_Data", "Game_Data/readme.txt"]) == nil)
        #expect(layout(["Game.exe", "UnityPlayer.dll"]) == nil)
        #expect(layout(["Save_Data", "Save_Data/file1.sav"]) == nil)
    }

    /// A wrapped RPG Maker game is only a likely match. Unity, asked last, must not take it for a Unity tool beside it.
    @Test func neverOutranksAnotherEnginesMatch() throws {
        let folder = FileManager.default.temporaryDirectory.appending(path: "unity-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: folder.appending(path: "Release/Tool_Data"), withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        try Data().write(to: folder.appending(path: "Release/Tool_Data/globalgamemanagers"))
        try Data().write(to: folder.appending(path: "Release/Tool.exe"))
        let probe = FolderProbe(scanning: folder)
        #expect(EngineDetector.detect(probe, using: [UnityPlugin()])?.engine == .unity)
        #expect(EngineDetector.detect(probe, using: [LikelyPlugin(), UnityPlugin()])?.engine == LikelyPlugin.id)
    }

    @Test func prefersTheGameWithItsPlayerAndThenTheShallowest() {
        let found = layout(["Extras_Data", "Extras_Data/globalgamemanagers",
                            "Game", "Game/Game.exe", "Game/Game_Data", "Game/Game_Data/globalgamemanagers"])
        #expect(found?.root == "Game")
        let shallow = layout(["Game.exe", "Game_Data", "Game_Data/globalgamemanagers",
                              "Demo", "Demo/Demo.exe", "Demo/Demo_Data", "Demo/Demo_Data/globalgamemanagers"])
        #expect(shallow?.root == "")
        // Shallowest by folders, not by the length of the path.
        let byDepth = layout(["WindowsBuild", "WindowsBuild/Game.exe", "WindowsBuild/Game_Data", "WindowsBuild/Game_Data/globalgamemanagers",
                              "Extras", "Extras/Tool", "Extras/Tool/Tool.exe", "Extras/Tool/Tool_Data"],
                             deeper: ["Extras/Tool/Tool_Data": ["globalgamemanagers"]])
        #expect(byDepth?.root == "WindowsBuild")
    }

    @Test func findsTheMacBuildBesideIt() {
        let found = layout(["Game.exe", "Game_Data", "Game_Data/globalgamemanagers", "Game.app", "Game.app/Contents"],
                           deeper: ["Game.app/Contents": ["Resources"], "Game.app/Contents/Resources": ["Data"],
                                    "Game.app/Contents/Resources/Data": ["globalgamemanagers", "Managed"]])
        #expect(found?.macApp == "Game.app")
        let other = layout(["Game.exe", "Game_Data", "Game_Data/globalgamemanagers", "Other.app", "Other.app/Contents"])
        #expect(other?.macApp == nil)
    }

    /// SiNiSistar 2 keeps each build in its own folder.
    @Test func findsTheMacBuildInASiblingFolder() {
        let found = layout(["WIN", "WIN/Game.exe", "WIN/Game_Data", "WIN/Game_Data/globalgamemanagers", "MAC", "MAC/Game.app",
                            "MAC/Game.app/Contents"],
                           deeper: ["MAC/Game.app/Contents": ["Resources"], "MAC/Game.app/Contents/Resources": ["Data"],
                                    "MAC/Game.app/Contents/Resources/Data": ["globalgamemanagers"]])
        #expect(found?.root == "WIN")
        #expect(found?.macApp == "MAC/Game.app")
        let lower = layout(["WIN", "WIN/Game.exe", "WIN/Game_Data", "WIN/Game_Data/globalgamemanagers", "MAC", "MAC/Game.app",
                            "MAC/Game.app/contents"],
                           deeper: ["MAC/Game.app/contents": ["resources"], "MAC/Game.app/contents/resources": ["DATA"],
                                    "MAC/Game.app/contents/resources/DATA": ["GlobalGameManagers"]])
        #expect(lower?.macApp == "MAC/Game.app")
    }

    @Test func readsTheVersionFromEveryHeader() throws {
        let folder = FileManager.default.temporaryDirectory.appending(path: "unity-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        // A serialized file before format 22, its version after a 20-byte header.
        var old = Data(repeating: 0, count: 20)
        old.append(contentsOf: Array("2019.4.40f1".utf8) + [0, 5, 0, 0, 0])
        try old.write(to: folder.appending(path: "globalgamemanagers"))
        // A packed bundle: the format's own version, then the engine's.
        try Data(Array("UnityFS".utf8) + [0, 0, 0, 0, 8] + Array("5.x.x".utf8) + [0] + Array("6000.0.50f1".utf8) + [0])
            .write(to: folder.appending(path: "data.unity3d"))
        let probe = FolderProbe(scanning: folder)
        #expect(UnityLayout.version(probe, evidence: "globalgamemanagers") == "2019.4.40f1")
        #expect(UnityLayout.version(probe, evidence: "data.unity3d") == "6000.0.50f1")
        // Unity China adds a release suffix.
        try Data(Array("UnityFS".utf8) + [0, 0, 0, 0, 8] + Array("5.x.x".utf8) + [0] + Array("2021.3.21f1c1".utf8) + [0])
            .write(to: folder.appending(path: "china.unity3d"))
        #expect(UnityLayout.version(FolderProbe(scanning: folder), evidence: "china.unity3d") == "2021.3.21f1c1")
        #expect(UnityLayout.version(probe, evidence: "missing") == nil)
    }

    @Test func pluginExplainsAndRunsNowhere() throws {
        let plugin = UnityPlugin()
        #expect(plugin.descriptor.platforms.isEmpty)
        let folder = FileManager.default.temporaryDirectory.appending(path: "unity-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: folder.appending(path: "Game_Data/il2cpp_data"), withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        try Data().write(to: folder.appending(path: "Game_Data/globalgamemanagers"))
        try Data().write(to: folder.appending(path: "Game.exe"))
        let match = plugin.detect(FolderProbe(scanning: folder))
        #expect(match?.engine == .unity)
        #expect(match?.confidence == .likely)
        let note = plugin.compatibilityNote(for: Game(id: GameID("g"), title: "G"), folder: folder, dataDirectory: folder)
        #expect(note?.hasPrefix("VNDegen can't run Unity games.") == true)
        #expect(note?.contains("IL2CPP") == true)
    }
}

/// Another engine's likely match, such as RPG Maker's for a game inside a wrapper folder.
private struct LikelyPlugin: EnginePlugin {
    static let id = EngineID("likely")
    var descriptor: EngineDescriptor { EngineDescriptor(id: Self.id, displayName: "Likely", shortName: "Likely", platforms: [.macOS, .iOS]) }
    func detect(_ probe: FolderProbe) -> EngineMatch? { EngineMatch(engine: Self.id, confidence: .likely, evidence: "Release") }
    @MainActor func makeSession(_ launch: GameLaunch) throws -> any EngineSession { throw UnityError.cantRun }
}
