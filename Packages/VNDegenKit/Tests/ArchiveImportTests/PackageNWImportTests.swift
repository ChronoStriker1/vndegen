import Foundation
import RuntimePackages
import TestSupport
import Testing
@testable import ArchiveImport

private var fm: FileManager { .default }

private func scratch() throws -> URL {
    let url = TestFolders.root.appendingPathComponent("packagenw-\(UUID().uuidString)", isDirectory: true)
    try fm.createDirectory(at: url, withIntermediateDirectories: true)
    return url
}

private func tree(_ folder: URL) -> [String] {
    (fm.enumerator(atPath: folder.path)?.allObjects as? [String] ?? []).sorted()
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

private func zipped(_ files: [String: String], in folder: URL) throws -> Data {
    let content = folder.appendingPathComponent("package-\(UUID().uuidString)", isDirectory: true)
    for (path, text) in files {
        let url = content.appendingPathComponent(path)
        try fm.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(text.utf8).write(to: url)
    }
    let zip = folder.appendingPathComponent("package-\(UUID().uuidString).zip")
    try run("/usr/bin/ditto", ["-c", "-k", "--norsrc", content.path, zip.path])
    return try Data(contentsOf: zip)
}

private let game = ["index.html": "<html>", "data.json": "{}", "scripts/c3runtime.js": "runtime"]

@MainActor
private func importFolder(_ files: [String: Data], named name: String, in folder: URL) async throws -> URL {
    let content = folder.appendingPathComponent(name, isDirectory: true)
    for (path, data) in files {
        let url = content.appendingPathComponent(path)
        try fm.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try data.write(to: url)
    }
    let zip = folder.appendingPathComponent("\(name).zip")
    try run("/usr/bin/ditto", ["-c", "-k", "--norsrc", "--keepParent", content.path, zip.path])
    let games = folder.appendingPathComponent("Games")
    try fm.createDirectory(at: games, withIntermediateDirectories: true)
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([zip])
    importer.startImport()
    try await waitForImport(importer)
    guard case .finished = importer.step else {
        Issue.record("\(importer.step) \(importer.errorMessage ?? "")")
        return games.appendingPathComponent(name)
    }
    #expect(tree(games.appendingPathComponent(".importing")).isEmpty)
    return games.appendingPathComponent(name)
}

@MainActor @Test func unpacksAnNWjsGamesPackageFromItsInstaller() async throws {
    let folder = try scratch()
    let package = try zipped(game, in: folder)
    let built = SyntheticGOGInstaller.make(appName: "Vixen Game", entries: [
        SyntheticGOGInstaller.info(name: "Vixen Game", gameId: "300", rootGameId: "300"),
        .file("{app}\\VixenGame.exe", Array("nw".utf8)),
        .file("{app}\\nw.dll", Array("dll".utf8)),
        .file("{app}\\package.nw", Array(package)),
    ], setupName: "setup_vixen_game_1.6_(77)")
    let setup = folder.appendingPathComponent("setup_vixen_game_1.6_(77).exe")
    try built.setup.write(to: setup)
    let games = folder.appendingPathComponent("Games")
    try fm.createDirectory(at: games, withIntermediateDirectories: true)
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([setup])
    importer.startImport()
    try await waitForImport(importer)
    guard case let .finished(result) = importer.step else { Issue.record("\(importer.errorMessage ?? "")"); return }
    let unpacked = games.appendingPathComponent(result.folderName)
    #expect(tree(unpacked).filter { $0.hasPrefix("package.nw") } == [
        "package.nw", "package.nw/data.json", "package.nw/index.html", "package.nw/scripts", "package.nw/scripts/c3runtime.js",
    ])
    #expect(try String(contentsOf: unpacked.appendingPathComponent("package.nw/scripts/c3runtime.js"), encoding: .utf8) == "runtime")
    #expect(!tree(unpacked).contains { $0.hasPrefix(".package.nw") })
    #expect(tree(games.appendingPathComponent(".importing")).isEmpty)
}

@MainActor @Test func unpacksAZippedPackageBesideNWjsInAnArchive() async throws {
    let folder = try scratch()
    let unpacked = try await importFolder(["nw.pak": Data("pak".utf8), "package.nw": try zipped(game, in: folder)], named: "Vixens", in: folder)
    #expect(tree(unpacked) == [
        "nw.pak", "package.nw", "package.nw/data.json", "package.nw/index.html", "package.nw/scripts", "package.nw/scripts/c3runtime.js",
    ])
}

@MainActor @Test func keepsAPackageWithoutNWjsOrAPage() async throws {
    let folder = try scratch()
    let alone = try await importFolder(["package.nw": try zipped(game, in: folder), "readme.txt": Data("hi".utf8)], named: "Alone", in: folder)
    #expect(tree(alone) == ["package.nw", "readme.txt"])
    let pageless = try await importFolder(["nw.dll": Data(), "package.nw": try zipped(["main.js": "x"], in: folder)], named: "Pageless", in: folder)
    #expect(tree(pageless) == ["nw.dll", "package.nw"])
    var isFolder: ObjCBool = false
    #expect(fm.fileExists(atPath: pageless.appendingPathComponent("package.nw").path, isDirectory: &isFolder) && !isFolder.boolValue)
}
