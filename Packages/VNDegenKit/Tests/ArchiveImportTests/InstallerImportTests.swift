import Foundation
import RuntimePackages
import TestSupport
import Testing
@testable import ArchiveImport

private var fm: FileManager { .default }

private func scratch() throws -> URL {
    let url = TestFolders.root.appendingPathComponent("installerimport-\(UUID().uuidString)", isDirectory: true)
    try fm.createDirectory(at: url, withIntermediateDirectories: true)
    return url
}

private func tree(_ folder: URL) -> [String] {
    (fm.enumerator(atPath: folder.path)?.allObjects as? [String] ?? []).sorted()
}

private func base(sliced: Bool = false) -> SyntheticGOGInstaller.Built {
    SyntheticGOGInstaller.make(appName: "Orc Game - Flags", entries: [
        SyntheticGOGInstaller.info(name: "Orc Game: Flags", gameId: "100", rootGameId: "100"),
        .file("{app}\\Orc Game.exe", Array("exe".utf8)),
        .galaxy("data.xp3", Array(repeating: 7, count: 100_000), parts: 2),
    ], sliced: sliced, setupName: "setup_orc_game_1.0_(42)")
}

private let addOn = SyntheticGOGInstaller.make(appName: "Orc Game - Flags UNRATED", entries: [
    SyntheticGOGInstaller.info(name: "Orc Game: Flags UNRATED", gameId: "101", rootGameId: "100"),
    .galaxy("patch.xp3", Array("patch".utf8), parts: 1),
], setupName: "setup_orc_game_unrated_1.0_(42)")

private func put(_ built: SyntheticGOGInstaller.Built, as stem: String, in folder: URL) throws -> URL {
    let setup = folder.appendingPathComponent("\(stem).exe")
    try built.setup.write(to: setup)
    if let slice = built.slice { try slice.write(to: folder.appendingPathComponent("\(stem)-1.bin")) }
    return setup
}

@MainActor
private func waitForImport(_ importer: ArchiveImporter) async throws {
    let deadline = ContinuousClock.now + .seconds(30)
    while importer.step == .extracting || importer.step == .opening {
        try #require(ContinuousClock.now < deadline, "import finished in time")
        try await Task.sleep(for: .milliseconds(10))
    }
}

private func run(_ tool: String, _ arguments: [String]) throws {
    let process = Process()
    process.executableURL = URL(fileURLWithPath: tool)
    process.arguments = arguments
    process.standardOutput = FileHandle.nullDevice
    try process.run()
    process.waitUntilExit()
    try #require(process.terminationStatus == 0, "\(tool) \(arguments)")
}

@Test func groupsAnInstallerWithItsPartsAndAddOnBesideIt() throws {
    let folder = try scratch()
    let setup = try put(base(sliced: true), as: "setup_orc_game_1.0_(42)", in: folder)
    let addOnSetup = try put(addOn, as: "setup_orc_game_unrated_1.0_(42)", in: folder)
    _ = try put(base(sliced: true), as: "setup_other_game_(7)", in: folder)
    try Data("readme".utf8).write(to: folder.appendingPathComponent("readme.txt"))
    let source = try ArchiveSource(picked: [setup])
    #expect(source.isInstaller)
    #expect(Set(source.volumes.map(\.lastPathComponent)) == [
        "setup_orc_game_1.0_(42).exe", "setup_orc_game_1.0_(42)-1.bin", "setup_orc_game_unrated_1.0_(42).exe",
    ])
    #expect(source.installers.filter(\.picked).map { source.volumes[$0.indexes[0]] } == [setup])
    #expect(try ArchiveSource(picked: [addOnSetup]).volumes.count == 3)
}

@Test func refusesInstallerPartsWithoutTheirSetup() {
    #expect(throws: ArchiveError.installerPartsOnly) {
        try ArchiveSource(picked: [URL(fileURLWithPath: "/nowhere/setup_game-1.bin")])
    }
    #expect(throws: ArchiveError.mixedWithInstaller) {
        try ArchiveSource(picked: [URL(fileURLWithPath: "/nowhere/setup.exe"), URL(fileURLWithPath: "/nowhere/readme.txt")])
    }
}

@Test func namesTheGameFolderAfterTheGOGTitle() {
    #expect(InstallerName.folderName(for: "Orc Game: Flags") == "Orc Game - Flags")
    #expect(InstallerName.folderName(for: "A/B:C") == "A-B-C")
    #expect(InstallerName("setup_orc_game_1.0_(42).exe")?.build == "42")
    #expect(InstallerName("setup_orc_game_1.0_(42)-12.bin")?.index == 12)
}

@MainActor @Test(arguments: [true, false])
func importsAGOGInstallerWithItsAddOn(includeAddOn: Bool) async throws {
    let folder = try scratch()
    let games = folder.appendingPathComponent("Games")
    try fm.createDirectory(at: games, withIntermediateDirectories: true)
    let setup = try put(base(sliced: true), as: "setup_orc_game_1.0_(42)", in: folder)
    _ = try put(addOn, as: "setup_orc_game_unrated_1.0_(42)", in: folder)
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([setup])
    try #require(importer.step == .reviewing, "\(importer.errorMessage ?? "")")
    #expect(importer.folderName == "Orc Game - Flags")
    #expect(importer.contents?.installer?.addOns.map(\.title) == ["Orc Game: Flags UNRATED"])
    #expect(importer.contents?.fileCount == 5)
    importer.includeAddOns = includeAddOn
    #expect(importer.contents?.fileCount == (includeAddOn ? 5 : 3))
    importer.startImport()
    try await waitForImport(importer)
    guard case let .finished(result) = importer.step else { Issue.record("\(importer.errorMessage ?? "")"); return }
    #expect(result.folderName == "Orc Game - Flags")
    let expected = ["Orc Game.exe", "data.xp3", "goggame-100.info"] + (includeAddOn ? ["goggame-101.info", "patch.xp3"] : [])
    #expect(tree(games.appendingPathComponent("Orc Game - Flags")) == expected.sorted())
    #expect(try Data(contentsOf: games.appendingPathComponent("Orc Game - Flags/data.xp3")) == Data(repeating: 7, count: 100_000))
}

@MainActor @Test func refusesAnAddOnChosenAlone() async throws {
    let folder = try scratch()
    let setup = try put(addOn, as: "setup_orc_game_unrated_1.0_(42)", in: folder)
    let importer = ArchiveImporter(gamesRoot: try scratch())
    await importer.choose([setup])
    #expect(importer.step == .choosing)
    #expect(importer.errorMessage == ArchiveError.addOnAlone("Orc Game: Flags UNRATED").message)
}

@MainActor @Test func refusesAnExecutableThatIsNotAnInstaller() async throws {
    let folder = try scratch()
    let exe = folder.appendingPathComponent("Game.exe")
    try Data(repeating: 0x4d, count: 4096).write(to: exe)
    let importer = ArchiveImporter(gamesRoot: try scratch())
    await importer.choose([exe])
    #expect(importer.errorMessage == ArchiveError.installer(.notAnInstaller).message)
}

@MainActor @Test func deletesOnlyTheInstallerFilesItUnpacked() async throws {
    let games = try scratch()
    let setup = try put(base(sliced: true), as: "setup_orc_game_1.0_(42)", in: games)
    _ = try put(addOn, as: "setup_orc_game_unrated_1.0_(42)", in: games)
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([setup])
    #expect(importer.canDeleteArchive)
    #expect(importer.deleteArchiveAfterImport)
    importer.includeAddOns = false
    importer.startImport()
    try await waitForImport(importer)
    #expect(importer.step.isInstallerFinished)
    #expect((try? fm.contentsOfDirectory(atPath: games.path))?.sorted() == [".importing", "Orc Game - Flags", "setup_orc_game_unrated_1.0_(42).exe"])
}

@MainActor @Test func unpacksAnInstallerInsideAnArchive() async throws {
    let folder = try scratch()
    let content = folder.appendingPathComponent("Orc Game")
    try fm.createDirectory(at: content, withIntermediateDirectories: true)
    _ = try put(base(), as: "setup_orc_game_1.0_(42)", in: content)
    try Data("link".utf8).write(to: content.appendingPathComponent("GOG-Games.url"))
    let zip = folder.appendingPathComponent("Orc Game.zip")
    try run("/usr/bin/ditto", ["-c", "-k", "--norsrc", "--keepParent", content.path, zip.path])
    let games = folder.appendingPathComponent("Games")
    try fm.createDirectory(at: games, withIntermediateDirectories: true)
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([zip])
    importer.startImport()
    try await waitForImport(importer)
    #expect(importer.step == .nested(fileName: "setup_orc_game_1.0_(42).exe"))
    #expect(importer.nestedSource?.isInstaller == true)
    importer.extractNested()
    try await waitForImport(importer)
    #expect(importer.step.isInstallerFinished, "\(importer.errorMessage ?? "")")
    #expect(tree(games.appendingPathComponent("Orc Game")) == ["Orc Game.exe", "data.xp3", "goggame-100.info"])
    #expect(tree(games.appendingPathComponent(".importing")).isEmpty)
}

@MainActor @Test func importsAnISODiscImage() async throws {
    let folder = try scratch()
    let disc = folder.appendingPathComponent("disc")
    try fm.createDirectory(at: disc.appendingPathComponent("game"), withIntermediateDirectories: true)
    try Data("script".utf8).write(to: disc.appendingPathComponent("game/nscript.dat"))
    try Data("[autorun]".utf8).write(to: disc.appendingPathComponent("autorun.inf"))
    let iso = folder.appendingPathComponent("Disc Game.iso")
    try run("/usr/bin/hdiutil", ["makehybrid", "-quiet", "-iso", "-joliet", "-default-volume-name", "DISC", "-o", iso.path, disc.path])
    let games = folder.appendingPathComponent("Games")
    try fm.createDirectory(at: games, withIntermediateDirectories: true)
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([iso])
    try #require(importer.step == .reviewing, "\(importer.errorMessage ?? "")")
    #expect(importer.folderName == "Disc Game")
    importer.startImport()
    try await waitForImport(importer)
    #expect(importer.step.isInstallerFinished, "\(importer.errorMessage ?? "")")
    #expect(tree(games.appendingPathComponent("Disc Game")).contains("game/nscript.dat"))
}

@Test func listsInstallersReadyToImport() throws {
    let folder = try scratch()
    _ = try put(base(sliced: true), as: "setup_orc_game_1.0_(42)", in: folder)
    let listed = LocalArchives.scan([folder])
    #expect(listed.map(\.fileName) == ["setup_orc_game_1.0_(42).exe"])
    #expect(listed.first?.parts == 2)
}

private extension ArchiveImporter.Step {
    var isInstallerFinished: Bool { if case .finished = self { true } else { false } }
}
