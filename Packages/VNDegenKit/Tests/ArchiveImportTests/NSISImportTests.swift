import Foundation
import TestSupport
import Testing
@testable import ArchiveImport

private var fm: FileManager { .default }

private func scratch() throws -> URL {
    let url = TestFolders.root.appendingPathComponent("nsisimport-\(UUID().uuidString)", isDirectory: true)
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

private func run(_ tool: String, _ arguments: [String], in folder: URL? = nil) throws {
    let process = Process()
    process.executableURL = URL(fileURLWithPath: tool)
    process.arguments = arguments
    if let folder { process.currentDirectoryURL = folder }
    process.standardOutput = FileHandle.nullDevice
    try process.run()
    process.waitUntilExit()
    try #require(process.terminationStatus == 0, "\(tool) \(arguments)")
}

private func le32(_ value: UInt32) -> Data { withUnsafeBytes(of: value.littleEndian) { Data($0) } }

private func asar(_ files: [(String, String)], decoys: String) -> Data {
    var body = Data()
    var entries: [String] = []
    var folders: [String: [String]] = [:]
    for (path, text) in files {
        let parts = path.split(separator: "/").map(String.init)
        let entry = "\"\(parts.last!)\":{\"size\":\(text.utf8.count),\"offset\":\"\(body.count)\"}"
        body.append(Data(text.utf8))
        if parts.count == 1 { entries.append(entry) } else { folders[parts.dropLast().joined(separator: "/"), default: []].append(entry) }
    }
    func folder(_ name: String) -> String {
        let children = folders.filter { $0.key.split(separator: "/").dropLast().joined(separator: "/") == name && $0.key != name }.keys.sorted()
        let own = folders[name] ?? []
        let nested = children.map { "\"\($0.split(separator: "/").last!)\":\(folder($0))" }
        return "{\"files\":{\((own + nested).joined(separator: ","))}}"
    }
    let tops = Set(folders.keys.map { String($0.split(separator: "/")[0]) })
    let json = "{\"files\":{\((entries + tops.sorted().map { "\"\($0)\":\(folder($0))" } + [decoys]).joined(separator: ","))}}"
    let jsonData = Data(json.utf8)
    let padded = (jsonData.count + 3) & ~3
    let headerPickle = 4 + padded
    var file = le32(4)
    file += le32(UInt32(headerPickle + 4))
    file += le32(UInt32(headerPickle))
    file += le32(UInt32(jsonData.count))
    file += jsonData
    file += Data(count: padded - jsonData.count)
    file += body
    return file
}

private func electronApp(in folder: URL) throws -> URL {
    let app = folder.appendingPathComponent("app-\(UUID().uuidString)", isDirectory: true)
    try fm.createDirectory(at: app.appendingPathComponent("resources"), withIntermediateDirectories: true)
    try Data(count: 4096).write(to: app.appendingPathComponent("School Game.exe"))
    try Data("icu".utf8).write(to: app.appendingPathComponent("icudtl.dat"))
    let decoys = "\"\(String(repeating: "ab", count: 30))\":{\"size\":1073741824,\"offset\":\"0\"},\"license\":{\"size\":1.5e307,\"offset\":\"0\"}"
    try asar([
        ("package.json", "{\"main\":\"src/main.js\"}"), ("src/main.js", "main"),
        ("project/index.html", "<html>"), ("project/js/rmmz_core.js", "core"), ("project/data/System.json", "{}"),
    ], decoys: decoys).write(to: app.appendingPathComponent("resources/app.asar"))
    return app
}

private func installer(holding app: URL, in folder: URL, named name: String = "school.exe") throws -> URL {
    let payload = app.deletingLastPathComponent().appendingPathComponent("app-64-\(UUID().uuidString).7z")
    try run("/usr/bin/bsdtar", ["--format", "7zip", "-cf", payload.path, "."], in: app)
    let sevenZip = try Data(contentsOf: payload)
    var pe = Data(count: 0x400)
    pe[0] = 0x4d
    pe[1] = 0x5a
    pe.replaceSubrange(0x3c..<0x40, with: le32(0x40))
    pe.replaceSubrange(0x40..<0x44, with: Data("PE\0\0".utf8))
    pe[0x46] = 1
    let section = 0x40 + 24
    pe.replaceSubrange(section..<(section + 8), with: Data(".text\0\0\0".utf8))
    pe.replaceSubrange((section + 16)..<(section + 20), with: le32(0x200))
    pe.replaceSubrange((section + 20)..<(section + 24), with: le32(0x200))
    let header = Data(repeating: 0x5a, count: 100)
    let plugin = Data(repeating: 0x33, count: 300)
    let trailer = Data(repeating: 0x44, count: 200)
    var blocks = Data()
    for (block, compressed) in [(header, true), (plugin, true), (sevenZip, false), (trailer, true)] {
        blocks += le32(UInt32(block.count) | (compressed ? 0x8000_0000 : 0))
        blocks += block
    }
    var file = pe
    file += le32(4)
    file += le32(0xdead_beef)
    file += Data("NullsoftInst".utf8)
    file += le32(UInt32(header.count))
    file += le32(UInt32(28 + blocks.count))
    file += blocks
    let exe = folder.appendingPathComponent(name)
    try file.write(to: exe)
    return exe
}

@Test func findsTheSevenZipInsideAnNSISInstaller() throws {
    let folder = try scratch()
    let exe = try installer(holding: try electronApp(in: folder), in: folder)
    #expect(NSISPayload.offset(in: exe) == 1464)
    try Data("MZ not an installer".utf8).write(to: folder.appendingPathComponent("plain.exe"))
    #expect(NSISPayload.offset(in: folder.appendingPathComponent("plain.exe")) == nil)
    var broken = try Data(contentsOf: exe)
    broken[0x400 + 8] = 0x58
    try broken.write(to: folder.appendingPathComponent("broken.exe"))
    #expect(NSISPayload.offset(in: folder.appendingPathComponent("broken.exe")) == nil)
}

@MainActor @Test func importsAnElectronGameFromItsNSISInstaller() async throws {
    let folder = try scratch()
    let exe = try installer(holding: try electronApp(in: folder), in: folder)
    let games = folder.appendingPathComponent("Games")
    try fm.createDirectory(at: games, withIntermediateDirectories: true)
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([exe])
    #expect(importer.pickedPackedGame)
    importer.startImport()
    try await waitForImport(importer)
    guard case let .finished(result) = importer.step else { Issue.record("\(importer.step) \(importer.errorMessage ?? "")"); return }
    let unpacked = games.appendingPathComponent(result.folderName)
    #expect(tree(unpacked) == [
        "School Game.exe", "icudtl.dat", "resources", "resources/app", "resources/app/package.json", "resources/app/project",
        "resources/app/project/data", "resources/app/project/data/System.json", "resources/app/project/index.html",
        "resources/app/project/js", "resources/app/project/js/rmmz_core.js", "resources/app/src", "resources/app/src/main.js",
    ])
    #expect(try String(contentsOf: unpacked.appendingPathComponent("resources/app/project/js/rmmz_core.js"), encoding: .utf8) == "core")
    #expect(tree(games.appendingPathComponent(".importing")).isEmpty)
}

@MainActor @Test func unpacksAnNSISInstallerInsideAnArchiveBesideItsReadme() async throws {
    let folder = try scratch()
    let content = folder.appendingPathComponent("School Game 1.01", isDirectory: true)
    try fm.createDirectory(at: content, withIntermediateDirectories: true)
    _ = try installer(holding: try electronApp(in: folder), in: content, named: "midnightschool.exe")
    try Data("read me".utf8).write(to: content.appendingPathComponent("readme.txt"))
    let zip = folder.appendingPathComponent("School Game.zip")
    try run("/usr/bin/ditto", ["-c", "-k", "--norsrc", "--keepParent", content.path, zip.path])
    let games = folder.appendingPathComponent("Games")
    try fm.createDirectory(at: games, withIntermediateDirectories: true)
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([zip])
    importer.startImport()
    try await waitForImport(importer)
    #expect(importer.step == .nested(fileName: "midnightschool.exe"))
    #expect(importer.nestedSource?.isPackedGame == true)
    importer.extractNested()
    try await waitForImport(importer)
    guard case let .finished(result) = importer.step else { Issue.record("\(importer.errorMessage ?? "")"); return }
    #expect(result.folderName == "School Game")
    let unpacked = games.appendingPathComponent("School Game")
    #expect(tree(unpacked).filter { !$0.hasPrefix("resources/app/") } == [
        "School Game.exe", "icudtl.dat", "readme.txt", "resources", "resources/app",
    ])
    #expect(fm.fileExists(atPath: unpacked.appendingPathComponent("resources/app/project/index.html").path))
    #expect(tree(games.appendingPathComponent(".importing")).isEmpty)
}

@MainActor @Test func leavesAnAsarWithoutAnRPGMakerGamePacked() async throws {
    let folder = try scratch()
    let app = folder.appendingPathComponent("Novel", isDirectory: true)
    try fm.createDirectory(at: app.appendingPathComponent("resources"), withIntermediateDirectories: true)
    try asar([("index.html", "<html>"), ("tyrano/tyrano.js", "t")], decoys: "\"x\":{\"size\":1,\"offset\":\"0\"}")
        .write(to: app.appendingPathComponent("resources/app.asar"))
    try ArchiveExtractor.unpackWebArchives(in: app)
    #expect(tree(app) == ["resources", "resources/app.asar"])
}
