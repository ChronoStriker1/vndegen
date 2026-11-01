import Foundation
import Testing
import VNCore
@testable import EngineArtemis

@Suite struct ArtemisLayoutTests {
    private func folder(_ files: [String: Data]) throws -> URL {
        let root = FileManager.default.temporaryDirectory.appending(path: "artemis-\(UUID().uuidString)")
        for (path, data) in files {
            let url = root.appending(path: path)
            try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try data.write(to: url)
        }
        return root
    }

    @Test func findsTheBaseArchiveAFolderDown() throws {
        let root = try folder([
            "Game/root.pfs": Data("pf8 index".utf8),
            "Game/root.pfs.000": Data("pf8 patch".utf8),
            "Game/Game.exe": Data(),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let layout = try #require(ArtemisLayout(scanning: root))
        #expect(layout == ArtemisLayout(root: "Game", data: .archive("Game/root.pfs", version: .pf8)))
        #expect(layout.saveVersion == 1002)
        #expect(ArtemisPlugin().detect(FolderProbe(scanning: root))?.evidence == "Game/root.pfs")
    }

    @Test func olderArchivesSaveInVersion1000() throws {
        let root = try folder(["data.pfs": Data("pf2 index".utf8)])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(ArtemisLayout(scanning: root)?.saveVersion == 1000)
    }

    @Test func findsAnUnpackedGameAndIgnoresOtherFiles() throws {
        let root = try folder([
            "notes.pfs": Data("not an archive".utf8),
            "system.ini": Data("[WINDOWS]\nWIDTH = 1280\nBOOT = system/first.iet\n".utf8),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(ArtemisLayout(scanning: root) == ArtemisLayout(root: "", data: .folder))
        let other = try folder(["system.ini": Data("[Settings]\nvolume=3\n".utf8)])
        defer { try? FileManager.default.removeItem(at: other) }
        #expect(ArtemisLayout(scanning: other) == nil)
    }
}

@Suite struct ArtemisSettingsTests {
    @Test func readsTheWindowsSection() throws {
        let ini = "[IOS]\nWIDTH = 960\n[WINDOWS]\nWIDTH = 1024\nHEIGHT = 768\n//SAVEPATH = Old\nSAVEPATH = Studio\\ゲーム\nSAVEPATH_CSIDL = 26\n"
        let settings = ArtemisSettings(ini: try #require(ini.data(using: ArtemisSettings.windowsJapanese)))
        #expect(settings.width == 1024 && settings.height == 768)
        #expect(settings.savePath == "Studio/ゲーム")
        #expect(settings.inUserFolder)
        // ゲ is 83 51, ー is 81 5B, and ム is 83 80 in Shift_JIS, which cp1252 reads as ƒQ, the undefined 81 kept as
        // U+0081 followed by [, and ƒ€.
        #expect(settings.windowsSavePath == "Studio/ƒQ\u{81}[ƒ€")
    }

    @Test func defaultsToTheEnginesFolder() {
        let settings = ArtemisSettings(ini: Data("[WINDOWS]\nBOOT = system/first.iet\n".utf8))
        #expect(settings.savePath == "save" && !settings.inUserFolder && settings.windowsSavePath == nil)
    }
}

@Suite struct ArtemisSaveLocationTests {
    let directory = URL(filePath: "/data/game/saves/", directoryHint: .isDirectory)
    let spellings = ["Studio/Game", "Studio/Gâme"]

    @Test func mapsAPCBackupInEitherSpellingAndAnyCase() {
        #expect(ArtemisPlugin.saveRelativePath("C:/Users/x/AppData/Roaming/STUDIO/game/qsave01.dat", directory: directory,
                                               spellings: spellings) == "STUDIO/game/qsave01.dat")
        #expect(ArtemisPlugin.saveRelativePath("C:\\Users\\x\\AppData\\Roaming\\Studio\\Gâme\\system.dat", directory: directory,
                                               spellings: spellings) == "Studio/Gâme/system.dat")
        #expect(ArtemisPlugin.saveRelativePath("/data/game/saves/Studio/Game/saveg.dat", directory: directory,
                                               spellings: spellings) == "Studio/Game/saveg.dat")
        #expect(ArtemisPlugin.saveRelativePath("C:/Games/Studio/Game.exe", directory: directory, spellings: spellings) == nil)
        #expect(ArtemisPlugin.saveRelativePath("C:/Games/Studio/Game/", directory: directory, spellings: spellings) == nil)
    }
}

@Suite struct ArtemisInputTests {
    @Test func buttonsPressTheKeysArtemisGamesBind() {
        var router = ArtemisInputRouter()
        #expect(router.translate(.button(.confirm, pressed: true)) == [.key(VirtualKey.return, down: true)])
        #expect(router.translate(.button(.menu, pressed: true)) == [.click(VirtualKey.rightButton, down: true)])
        #expect(router.translate(.button(.menu, pressed: false)) == [.click(VirtualKey.rightButton, down: false)])
        #expect(router.translate(.button(.pageDown, pressed: true)) == [.key(VirtualKey.wheelDown, down: true)])
        #expect(router.releaseAll().count == 2)
    }

    @Test func aKeyHeldByTwoButtonsGoesUpWithTheLast() {
        var router = ArtemisInputRouter()
        #expect(router.translate(.button(.confirm, pressed: true)).count == 1)
        #expect(router.translate(.key(KeyboardKey(key: "Enter", code: "Enter", keyCode: 13), pressed: true)).isEmpty)
        #expect(router.translate(.button(.confirm, pressed: false)).isEmpty)
        #expect(router.translate(.key(KeyboardKey(key: "Enter", code: "Enter", keyCode: 13), pressed: false))
            == [.key(VirtualKey.return, down: false)])
    }

    @Test func theWheelPressesItsKeys() {
        var router = ArtemisInputRouter()
        #expect(router.scroll(ArtemisInputRouter.pointsPerNotch) == [.key(VirtualKey.wheelDown, down: true), .key(VirtualKey.wheelDown, down: false)])
        #expect(router.scroll(-ArtemisInputRouter.pointsPerNotch * 2).filter { $0 == .key(VirtualKey.wheelUp, down: true) }.count == 2)
    }

    @Test func usagesBecomeVirtualKeys() {
        #expect(VirtualKey.forUsage(4) == 65) // A
        #expect(VirtualKey.forUsage(39) == 48) // 0
        #expect(VirtualKey.forUsage(73) == VirtualKey.insert)
        #expect(VirtualKey.forUsage(58) == 112) // F1
        #expect(KeyboardKey(key: "a", code: "KeyA", keyCode: 65).virtualKey == 65)
    }
}

@MainActor
@Suite struct ArtemisPromptTests {
    private func key(_ code: String) -> ControllerEvent {
        .key(KeyboardKey(key: "", code: code, keyCode: 0), pressed: true)
    }

    @Test func keysFromTheKeyboardAndMouseLayoutStandForButtons() {
        #expect(ArtemisPrompt.button(for: key("Space")) == .confirm)
        #expect(ArtemisPrompt.button(for: key("KeyE")) == .cancel)
        #expect(ArtemisPrompt.button(for: key("Digit3")) == .left)
        #expect(ArtemisPrompt.button(for: key("KeyQ")) == nil)
        #expect(ArtemisPrompt.button(for: .gamepad(.a, pressed: true)) == .confirm)
        #expect(ArtemisPrompt.button(for: .button(.confirm, pressed: false)) == nil)
    }

    @Test func aChoiceWithoutCancelStaysOnCancel() {
        var answers: [Bool] = []
        let prompt = ArtemisPrompt(title: "", message: "", cancels: false, text: nil) { accepted, _ in answers.append(accepted) }
        prompt.handle(.button(.cancel, pressed: true))
        #expect(answers.isEmpty)
        prompt.handle(.button(.confirm, pressed: true))
        prompt.handle(.button(.confirm, pressed: true))
        #expect(answers == [true])
    }

    @Test func typedTextIsTheAnswer() {
        var answers: [String?] = []
        let prompt = ArtemisPrompt(title: "Name", message: "", cancels: false, text: "An") { _, text in answers.append(text) }
        prompt.type(.text("na"))
        prompt.type(.deleteBackward)
        prompt.handle(.button(.cancel, pressed: true))
        #expect(answers.isEmpty)
        prompt.type(.newline)
        #expect(answers == ["Ann"])
    }

    @Test func cancellingAKeyboardGivesNoText() {
        var answers: [(Bool, String?)] = []
        let prompt = ArtemisPrompt(title: "", message: "Name", cancels: true, text: "An") { answers.append(($0, $1)) }
        prompt.handle(key("Escape"))
        #expect(answers.count == 1)
        #expect(answers[0].0 == false)
        #expect(answers[0].1 == nil)
    }
}
