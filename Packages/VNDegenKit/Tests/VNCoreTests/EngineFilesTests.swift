import Foundation
import Testing
@testable import VNCore

/// A plugin with a fixed list, which finds its game by `js/rmmz_core.js`.
private struct ListPlugin: EnginePlugin {
    var list: EngineFileList? = EngineFileList(
        files: ["Game.exe", "nw.dll", "swiftshader/libEGL.dll"], filesOfAtLeast: ["credits.html": 100],
        fileEndings: ["locales": [".pak"]], folders: ["pnacl", "lib/py3-linux"])
    var descriptor: EngineDescriptor { EngineDescriptor(id: EngineID("list"), displayName: "List", shortName: "L", platforms: [.macOS, .iOS]) }
    func detect(_ probe: FolderProbe) -> EngineMatch? {
        probe.contains("js/rmmz_core.js") ? EngineMatch(engine: descriptor.id, confidence: .certain, evidence: "js/rmmz_core.js") : nil
    }
    func makeSession(_ launch: GameLaunch) throws -> any EngineSession { fatalError() }
    var listsEngineFiles: Bool { true }
    func engineFileList(_ probe: FolderProbe) -> EngineFileList? { list }
}

private struct Scratch {
    let base = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
    var paths: LibraryPaths { LibraryPaths(gamesRoot: base.appendingPathComponent("Games"), support: base.appendingPathComponent("Support")) }
    var game: URL { paths.gamesRoot.appendingPathComponent("Game", isDirectory: true) }

    func write(_ path: String, size: Int = 1, in folder: URL? = nil) throws {
        let url = (folder ?? game).appendingPathComponent(path)
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(count: size).write(to: url)
    }

    func exists(_ path: String) -> Bool { FileManager.default.fileExists(atPath: game.appendingPathComponent(path).path) }

    @MainActor func library(_ plugin: ListPlugin = ListPlugin()) async -> LibraryStore {
        let library = LibraryStore(paths: paths, engines: [plugin])
        library.deletesToTrash = false
        await library.refresh()
        return library
    }

    func writeGame() throws {
        for path in ["js/rmmz_core.js", "Game.exe", "NW.DLL", "credits.html", "locales/en.pak", "locales/game.json", "swiftshader/libEGL.dll",
                     "pnacl/a.nexe", "pnacl/deep/b.nexe", "save/file1.rmmzsave", "Other.exe", "winmm.dll", "lib/greenworks.node",
                     "lib/py3-linux/python"] {
            try write(path)
        }
    }
}

private let gameID = GameID("Game")

@Test func aListNamesOnlyWhatItSaysInAnyCase() throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.writeGame()
    let found = EngineFiles.files(ListPlugin().list!, in: scratch.game).map(\.path).sorted()
    // swiftshader holds nothing else, so it goes as a folder. locales and lib hold other files, so those stay.
    #expect(found == ["Game.exe", "NW.DLL", "lib/py3-linux", "locales/en.pak", "pnacl", "swiftshader"])
}

@Test func aFileBelowItsSizeStays() throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.write("credits.html", size: 99)
    #expect(EngineFiles.files(ListPlugin().list!, in: scratch.game).isEmpty)
    try scratch.write("credits.html", size: 100)
    #expect(EngineFiles.files(ListPlugin().list!, in: scratch.game).map(\.path) == ["credits.html"])
}

@Test func aFolderReportsItsSizeAndFileCount() throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.write("pnacl/a", size: 10)
    try scratch.write("pnacl/deep/b", size: 20)
    let file = try #require(EngineFiles.files(ListPlugin().list!, in: scratch.game).first)
    #expect(file.size == 30 && file.fileCount == 2)
}

@Test func linksAreNeverOfferedOrFollowed() throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    let outside = scratch.base.appendingPathComponent("outside", isDirectory: true)
    try scratch.write("keep.pak", in: outside)
    try scratch.write("js/rmmz_core.js")
    let fm = FileManager.default
    try fm.createSymbolicLink(at: scratch.game.appendingPathComponent("locales"), withDestinationURL: outside)
    try fm.createSymbolicLink(at: scratch.game.appendingPathComponent("pnacl"), withDestinationURL: outside)
    try fm.createSymbolicLink(at: scratch.game.appendingPathComponent("Game.exe"), withDestinationURL: outside.appendingPathComponent("keep.pak"))
    #expect(EngineFiles.files(ListPlugin().list!, in: scratch.game).isEmpty)
}

@Test func anICloudPlaceholderCountsAsItsFileAtItsRealSize() throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    let plist = try PropertyListSerialization.data(fromPropertyList: ["NSURLFileSizeKey": 5000, "NSURLNameKey": "nw.dll"], format: .binary, options: 0)
    try FileManager.default.createDirectory(at: scratch.game, withIntermediateDirectories: true)
    try plist.write(to: scratch.game.appendingPathComponent(".nw.dll.icloud"))
    let file = try #require(EngineFiles.files(ListPlugin().list!, in: scratch.game, iCloud: true).first)
    #expect(file.path == "nw.dll" && file.stored == ".nw.dll.icloud" && file.size == 5000)
    // Outside iCloud the same name is a hidden file nobody named.
    #expect(EngineFiles.files(ListPlugin().list!, in: scratch.game, iCloud: false).isEmpty)
}

/// The one place that decides, for each kind of place.
@Test func placesAreSortedAsTheyShouldBe() {
    func place(_ path: String, local: Bool = true, readOnly: Bool = false, ubiquitous: Bool = false) -> EngineFiles.Place {
        EngineFiles.place(path: path, isLocal: local, isReadOnly: readOnly, isUbiquitous: ubiquitous)
    }
    #expect(place("/Users/a/Games/VNDegen/Game") == .local)
    // On My iPhone and other cloud providers share this path, so isUbiquitous is what tells them apart.
    #expect(place("/private/var/mobile/Containers/Shared/AppGroup/X/File Provider Storage/Games/Game") == .local)
    #expect(place("/Users/a/Library/Mobile Documents/com~apple~CloudDocs/Games/Game", ubiquitous: true) == .iCloud)
    #expect(place("/private/var/mobile/Library/Mobile Documents/com~apple~CloudDocs/Games/Game", ubiquitous: true) == .iCloud)
    for refused in [place("/Volumes/share/Game", local: false),
                    place("/Volumes/share/Library/Mobile Documents/Game", local: false),
                    place("/private/var/mobile/Library/LiveFiles/com.apple.filesystems.smbclientd/x/Game"),
                    place("/Volumes/disk/Game", readOnly: true),
                    place("/Users/a/Library/CloudStorage/Dropbox/Game"),
                    place("/private/var/mobile/Containers/Shared/AppGroup/X/File Provider Storage/Game", ubiquitous: true)] {
        guard case .refused = refused else { Issue.record("not refused: \(refused)"); continue }
    }
}

@Test @MainActor func removingTakesOnlyThePlanAndSavesStay() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.writeGame()
    let library = await scratch.library()
    let plan = await library.engineFilePlan(for: gameID)
    #expect(plan.problem == nil && plan.files.count == 6)
    let result = await library.removeEngineFiles(plan.files, of: gameID)
    #expect(result.removed.count == 6 && result.failed.isEmpty)
    for gone in ["Game.exe", "NW.DLL", "locales/en.pak", "pnacl", "swiftshader", "lib/py3-linux"] { #expect(!scratch.exists(gone)) }
    for kept in ["js/rmmz_core.js", "save/file1.rmmzsave", "locales/game.json", "Other.exe", "winmm.dll", "lib/greenworks.node", "credits.html"] {
        #expect(scratch.exists(kept))
    }
    #expect(library.game(gameID)?.detected?.engine == EngineID("list"))
    #expect(await library.engineFilePlan(for: gameID).files.isEmpty)
}

@Test @MainActor func aFileThatChangedAfterTheListStays() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.writeGame()
    let library = await scratch.library()
    let plan = await library.engineFilePlan(for: gameID)
    try scratch.write("Game.exe", size: 2)
    let result = await library.removeEngineFiles(plan.files, of: gameID)
    #expect(scratch.exists("Game.exe"))
    #expect(result.failed.map(\.path) == ["Game.exe"] && result.removed.count == plan.files.count - 1)
}

@Test @MainActor func aPartialFailureReportsBothLists() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.writeGame()
    let library = await scratch.library()
    let plan = await library.engineFilePlan(for: gameID)
    // A folder VNDegen can't write to holds one of them, so that removal fails and the rest go.
    let locked = scratch.game.appendingPathComponent("lib")
    try FileManager.default.setAttributes([.posixPermissions: 0o555], ofItemAtPath: locked.path)
    defer { try? FileManager.default.setAttributes([.posixPermissions: 0o755], ofItemAtPath: locked.path) }
    let result = await library.removeEngineFiles(plan.files, of: gameID)
    #expect(result.failed.map(\.path) == ["lib/py3-linux"])
    #expect(result.removed.count == plan.files.count - 1)
    #expect(scratch.exists("lib/py3-linux") && !scratch.exists("Game.exe"))
}

@Test @MainActor func aFolderVNDegenCantWriteToRemovesNothing() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.writeGame()
    let library = await scratch.library()
    let plan = await library.engineFilePlan(for: gameID)
    try FileManager.default.setAttributes([.posixPermissions: 0o555], ofItemAtPath: scratch.game.path)
    defer { try? FileManager.default.setAttributes([.posixPermissions: 0o755], ofItemAtPath: scratch.game.path) }
    #expect(await library.engineFilePlan(for: gameID).problem != nil)
    let result = await library.removeEngineFiles(plan.files, of: gameID)
    #expect(result.removed.isEmpty && result.failed.count == plan.files.count)
    #expect(scratch.exists("Game.exe"))
}

@Test @MainActor func aGameLinkedIntoTheLibraryIsRefused() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    let elsewhere = scratch.base.appendingPathComponent("Elsewhere", isDirectory: true)
    try scratch.write("js/rmmz_core.js", in: elsewhere)
    try scratch.write("Game.exe", in: elsewhere)
    try FileManager.default.createDirectory(at: scratch.paths.gamesRoot, withIntermediateDirectories: true)
    try FileManager.default.createSymbolicLink(at: scratch.game, withDestinationURL: elsewhere)
    let library = await scratch.library()
    let plan = await library.engineFilePlan(for: gameID)
    #expect(plan.problem != nil && plan.files.isEmpty)
}

/// The plugin must still find the game without the files, or the list is wrong and nothing is offered.
@Test @MainActor func aListThatWouldBreakDetectionOffersNothing() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.writeGame()
    var plugin = ListPlugin()
    plugin.list = EngineFileList(files: ["Game.exe", "js/rmmz_core.js"])
    let library = await scratch.library(plugin)
    #expect(await library.engineFilePlan(for: gameID).files.isEmpty)
}

/// An engine assigned by hand may not be what its list expects, as with a game packed inside its `.exe`.
@Test @MainActor func aGameTheEngineDidntDetectOffersNothing() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.write("Game.exe")
    try scratch.write("nw.dll")
    let library = await scratch.library()
    #expect(!library.offersEngineFiles(gameID))
    library.update(gameID) { $0.engineOverride = EngineID("list") }
    #expect(!library.offersEngineFiles(gameID))
    #expect(await library.engineFilePlan(for: gameID).files.isEmpty)
    let result = await library.removeEngineFiles([], of: gameID)
    #expect(result.removed.isEmpty)
    #expect(scratch.exists("Game.exe"))
}
