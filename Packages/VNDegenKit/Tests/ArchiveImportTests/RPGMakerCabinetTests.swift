import Foundation
import TestSupport
import Testing
@testable import ArchiveImport

private var fm: FileManager { .default }
private let fixtures = URL(fileURLWithPath: #filePath).deletingLastPathComponent().appendingPathComponent("Fixtures")
private let lzxCabinet = (try? Data(contentsOf: fixtures.appendingPathComponent("lzx.cab"))) ?? Data()
private let expectedFiles = [
    "dir1/file1": Data((String(repeating: " ", count: 26) + "file 1 contents\n" + String(repeating: "hello\n", count: 3)).utf8),
    "dir2/file2": Data((String(repeating: " ", count: 26) + "file 2 contents\n" + String(repeating: "hello\n", count: 6)).utf8),
    "empty": Data(),
    "zero": Data(count: 33000),
]

private func scratch() throws -> URL {
    let url = TestFolders.root.appendingPathComponent("vxacecab-\(UUID().uuidString)", isDirectory: true)
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

private func le32(_ value: UInt32) -> Data { withUnsafeBytes(of: value.littleEndian) { Data($0) } }

private func selfExtractor(marker: String = "Software\\Enterbrain\\RPGVX2", cabinet: Data = lzxCabinet, cabinetSize: Int? = nil) -> Data {
    var pe = Data(count: 0x400)
    pe[0] = 0x4d
    pe[1] = 0x5a
    pe.replaceSubrange(0x3c..<0x40, with: le32(0x40))
    pe.replaceSubrange(0x40..<0x44, with: Data("PE\0\0".utf8))
    pe[0x46] = 1
    pe.replaceSubrange(0x68..<0x6c, with: le32(0x200))
    pe.replaceSubrange(0x6c..<0x70, with: le32(0x200))
    let utf16 = Data(marker.utf16.flatMap { [UInt8($0 & 0xff), UInt8($0 >> 8)] })
    pe.replaceSubrange(0x200..<(0x200 + utf16.count), with: utf16)
    var body = cabinet
    if let cabinetSize { body.replaceSubrange(8..<12, with: le32(UInt32(cabinetSize))) }
    return pe + body
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

@Test func recognizesRPGMakerVXAcesSelfExtractor() throws {
    let folder = try scratch()
    let cases: [(String, Data, Bool)] = [
        ("SH_v180101.exe", selfExtractor(), true),
        ("other.exe", selfExtractor(marker: "Software\\Microsoft\\Cabinet"), false),
        ("short.exe", selfExtractor(cabinetSize: 100), false),
        ("plain.exe", Data("MZ".utf8) + Data(count: 100), false),
    ]
    for (name, data, packed) in cases {
        let url = folder.appendingPathComponent(name)
        try data.write(to: url)
        #expect(RPGMakerCabinet.isPacked(url) == packed, "\(name)")
    }
}

private func contents(of folder: URL) throws -> [String: Data] {
    var found: [String: Data] = [:]
    for path in tree(folder) {
        var isFolder: ObjCBool = false
        let url = folder.appendingPathComponent(path)
        if fm.fileExists(atPath: url.path, isDirectory: &isFolder), !isFolder.boolValue { found[path] = try Data(contentsOf: url) }
    }
    return found
}

@Test func decodesALZXCabinetMadeByMakecab() throws {
    let cabinet = try Cabinet(fixtures.appendingPathComponent("lzx.cab"), at: 0)
    #expect(Set(cabinet.files.map(\.path)) == Set(expectedFiles.keys))
    #expect(cabinet.directories == ["dir1", "dir2"] || cabinet.directories == ["dir2", "dir1"])
    var written: [String: Data] = [:]
    try cabinet.extract(from: fixtures.appendingPathComponent("lzx.cab")) { index, block in
        written[cabinet.files[index].path, default: Data()].append(contentsOf: block)
    }
    #expect(written == expectedFiles)
}

@MainActor @Test func aPickedSelfExtractorImportsItsGame() async throws {
    let folder = try scratch()
    let exe = folder.appendingPathComponent("SH_v180101.exe")
    try selfExtractor().write(to: exe)
    let games = folder.appendingPathComponent("Games")
    try fm.createDirectory(at: games, withIntermediateDirectories: true)
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([exe])
    #expect(importer.errorMessage == nil)
    #expect(importer.pickedPackedGame)
    importer.startImport()
    try await waitForImport(importer)
    guard case let .finished(result) = importer.step else { Issue.record("\(importer.step) \(importer.errorMessage ?? "")"); return }
    #expect(try contents(of: games.appendingPathComponent(result.folderName)) == expectedFiles)
    #expect(tree(games.appendingPathComponent(".importing")).isEmpty)
}

@MainActor @Test func aSelfExtractorInAnArchiveUnpacksBesideItsReadme() async throws {
    let folder = try scratch()
    let content = folder.appendingPathComponent("Succubus Game [RJ000001] Ver.1/Succubus Game", isDirectory: true)
    try fm.createDirectory(at: content, withIntermediateDirectories: true)
    try selfExtractor().write(to: content.appendingPathComponent("SG_v180101.exe"))
    try Data("read me".utf8).write(to: content.appendingPathComponent("readme.txt"))
    let zip = folder.appendingPathComponent("Succubus Game.zip")
    try run("/usr/bin/ditto", ["-c", "-k", "--norsrc", "--keepParent", content.deletingLastPathComponent().path, zip.path])
    let games = folder.appendingPathComponent("Games")
    try fm.createDirectory(at: games, withIntermediateDirectories: true)
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([zip])
    importer.startImport()
    try await waitForImport(importer)
    #expect(importer.step == .nested(fileName: "SG_v180101.exe"))
    #expect(importer.nestedSource?.isPackedGame == true)
    importer.extractNested()
    try await waitForImport(importer)
    guard case let .finished(result) = importer.step else { Issue.record("\(importer.step) \(importer.errorMessage ?? "")"); return }
    var expected = expectedFiles
    expected["readme.txt"] = Data("read me".utf8)
    #expect(try contents(of: games.appendingPathComponent(result.folderName)) == expected)
    #expect(tree(games.appendingPathComponent(".importing")).isEmpty)
}

@MainActor @Test func aSelfExtractorWithMSZIPSaysSo() async throws {
    let folder = try scratch()
    let exe = folder.appendingPathComponent("SH_v180101.exe")
    try selfExtractor(cabinet: try Data(contentsOf: fixtures.appendingPathComponent("mszip.cab"))).write(to: exe)
    let importer = ArchiveImporter(gamesRoot: folder.appendingPathComponent("Games"))
    await importer.choose([exe])
    #expect(importer.errorMessage == ArchiveError.packedGame(.unsupported("MSZIP compression")).message)
}

@Test func aCutShortCabinetIsDamaged() throws {
    let folder = try scratch()
    let url = folder.appendingPathComponent("cut.cab")
    try lzxCabinet.prefix(lzxCabinet.count - 40).write(to: url)
    let cabinet = try Cabinet(url, at: 0)
    #expect(throws: EnigmaPack.Failure.self) { try cabinet.extract(from: url) { _, _ in } }
}
