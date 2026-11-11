import Foundation
import TestSupport
import Testing
@testable import RuntimePackages

private let big = (0..<300_000).map { UInt8(truncatingIfNeeded: $0 &* 31 >> 3) }

private func folder() throws -> URL {
    let url = TestFolders.root.appending(path: "gameinstaller-\(UUID().uuidString)", directoryHint: .isDirectory)
    try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
    return url
}

private func write(_ built: SyntheticGOGInstaller.Built, named stem: String, in folder: URL) throws -> (URL, [URL]) {
    let setup = folder.appending(path: "\(stem).exe")
    try built.setup.write(to: setup)
    guard let slice = built.slice else { return (setup, []) }
    let part = folder.appending(path: "\(stem)-1.bin")
    try slice.write(to: part)
    return (setup, [part])
}

private func files(in folder: URL) -> [String] {
    (FileManager.default.enumerator(atPath: folder.path)?.allObjects as? [String] ?? []).sorted()
}

private let entries: [SyntheticGOGInstaller.Entry] = [
    SyntheticGOGInstaller.info(name: "Test: Game", gameId: "11", rootGameId: "11"),
    .file("{app}\\Game.exe", Array("exe".utf8)),
    .file("{app}\\Data\\a.txt", Array("hello".utf8)),
    .file("{tmp}\\skip.txt", Array("tmp".utf8)),
    .galaxy("data\\big.dat", big, parts: 3, check: "check_if_install('en-US#de-DE#', '64#', '')"),
    .galaxy("lang\\de.txt", Array("deutsch".utf8), parts: 1, check: "check_if_install('de-DE#', '', '')"),
    .galaxy("lang\\en.txt", Array("english".utf8), parts: 1, check: "check_if_install('en-US#', '', '')"),
    .galaxy("bin32\\x.dll", Array("32".utf8), parts: 1, check: "check_if_install('', '32#', '')"),
    .galaxy("__redist\\dx.exe", Array("dx".utf8), parts: 1),
]

@Suite struct GameInstallerTests {
    @Test(arguments: [false, true])
    func unpacksAGOGInstallerWithGalaxyParts(sliced: Bool) throws {
        let work = try folder()
        let built = SyntheticGOGInstaller.make(appName: "Test - Game", entries: entries, sliced: sliced, setupName: "setup_test_(5)")
        let (setup, slices) = try write(built, named: "setup_test_(5)", in: work)
        let game = try GameInstaller.open(setup: setup, slices: slices, preferredLanguages: ["de-DE"])
        #expect(game.title == "Test: Game")
        #expect(game.appName == "Test - Game")
        #expect(game.languages == ["de-DE", "en-US"])
        #expect(game.language == "de-DE")
        #expect(!game.isAddOn)
        #expect(game.fileCount == 5)
        let output = work.appending(path: "out")
        try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
        var bytes = 0, finished = 0
        try game.extract(setup: setup, slices: slices, into: output) { count, done in
            bytes += count
            if done { finished += 1 }
        }
        #expect(Int64(bytes) == game.byteCount)
        #expect(finished == game.fileCount)
        #expect(files(in: output) == ["Data", "Data/a.txt", "Data/big.dat", "Game.exe", "goggame-11.info", "lang", "lang/de.txt"])
        #expect(try Data(contentsOf: output.appending(path: "Data/big.dat")) == Data(big))
    }

    @Test func choosesTheInstallersDefaultLanguageWhenTheDeviceHasNone() throws {
        let work = try folder()
        let (setup, slices) = try write(SyntheticGOGInstaller.make(appName: "Test", entries: entries), named: "setup", in: work)
        let game = try GameInstaller.open(setup: setup, slices: slices, preferredLanguages: ["fr-FR"])
        #expect(game.language == "en-US")
        #expect(game.fileCount == 5)
    }

    @Test func choosesLanguagesByCodeScriptAndName() {
        #expect(GameInstaller.choose(from: ["de-DE", "en-US", "zh-Hans", "zh-Hant"], preferred: ["zh-Hant-TW"]) == "zh-Hant")
        #expect(GameInstaller.choose(from: ["english", "japanese"], preferred: ["ja-JP"]) == "japanese")
        #expect(GameInstaller.choose(from: ["ru-RU", "en-US"], preferred: []) == "en-US")
        #expect(GameInstaller.choose(from: ["ru-RU"], preferred: ["en-US"]) == "ru-RU")
    }

    @Test func recognizesAnAddOnByItsRootGame() throws {
        let work = try folder()
        let built = SyntheticGOGInstaller.make(appName: "Test UNRATED", entries: [
            SyntheticGOGInstaller.info(name: "Test UNRATED", gameId: "12", rootGameId: "11"),
            .galaxy("patch.xp3", Array("patch".utf8), parts: 1),
        ])
        let (setup, slices) = try write(built, named: "setup", in: work)
        let game = try GameInstaller.open(setup: setup, slices: slices)
        #expect(game.isAddOn)
        #expect(game.gog?.rootGameId == "11")
    }

    @Test func namesAMissingPart() throws {
        let work = try folder()
        let built = SyntheticGOGInstaller.make(appName: "Test", entries: entries, sliced: true, setupName: "setup_x")
        let setup = work.appending(path: "setup_x.exe")
        try built.setup.write(to: setup)
        #expect(throws: GameInstallerError.missingPart("setup_x-1.bin")) {
            try GameInstaller.open(setup: setup, slices: [])
        }
    }

    @Test func refusesADamagedPart() throws {
        let work = try folder()
        var data = SyntheticGOGInstaller.make(appName: "Test", entries: entries).setup
        let hello = try #require(data.firstRange(of: Data("hello".utf8)))
        data[hello.lowerBound] ^= 0xff
        let setup = work.appending(path: "setup.exe")
        try data.write(to: setup)
        let game = try GameInstaller.open(setup: setup, slices: [])
        let output = work.appending(path: "out")
        try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
        #expect {
            try game.extract(setup: setup, slices: [], into: output) { _, _ in }
        } throws: { error in
            if case GameInstallerError.damaged = error { true } else { false }
        }
    }

    @Test func refusesRepacks() throws {
        let work = try folder()
        let built = SyntheticGOGInstaller.make(appName: "Repack", publisher: "", entries: [
            .file("{tmp}\\ISDone.dll", Array("dll".utf8)),
            .file("{app}\\_Redist\\readme.txt", Array("text".utf8)),
        ])
        let (setup, _) = try write(built, named: "setup", in: work)
        #expect(throws: GameInstallerError.repack) { try GameInstaller.open(setup: setup, slices: []) }
    }

    @Test func refusesExecutablesThatAreNotInstallers() throws {
        let work = try folder()
        let file = work.appending(path: "Game.exe")
        try Data(repeating: 0x4d, count: 4096).write(to: file)
        #expect(throws: GameInstallerError.notAnInstaller) { try GameInstaller.open(setup: file, slices: []) }
        #expect(!GameInstaller.isInstaller(file))
    }
}
