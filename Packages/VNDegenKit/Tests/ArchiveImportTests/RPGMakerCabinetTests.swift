import Foundation
import TestSupport
import Testing
@testable import ArchiveImport

private var fm: FileManager { .default }

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

private func selfExtractor(marker: String = "Software\\Enterbrain\\RPGVX2", cabinetSize: Int? = nil) -> Data {
    var pe = Data(count: 0x400)
    pe[0] = 0x4d
    pe[1] = 0x5a
    pe.replaceSubrange(0x3c..<0x40, with: le32(0x40))
    pe.replaceSubrange(0x40..<0x44, with: Data("PE\0\0".utf8))
    pe[0x46] = 1
    pe.replaceSubrange(0x68..<0x6c, with: le32(0x200))
    pe.replaceSubrange(0x6c..<0x70, with: le32(0x200))
    pe.replaceSubrange(0x300..<(0x300 + marker.utf8.count), with: Data(marker.utf8))
    let body = Data(repeating: 0xab, count: 5000)
    let size = cabinetSize ?? 12 + body.count
    var file = pe
    file += Data("MSCF".utf8)
    file += le32(0)
    file += le32(UInt32(size))
    file += body
    return file
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

@MainActor @Test func aPickedSelfExtractorSaysToExtractItOnAPC() async throws {
    let folder = try scratch()
    let exe = folder.appendingPathComponent("SH_v180101.exe")
    try selfExtractor().write(to: exe)
    let importer = ArchiveImporter(gamesRoot: folder.appendingPathComponent("Games"))
    await importer.choose([exe])
    #expect(importer.errorMessage == ArchiveError.compressedRPGMakerGame.message)
}

@MainActor @Test func aSelfExtractorInAnArchiveStopsTheImportWithTheMessage() async throws {
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
    #expect(importer.errorMessage == ArchiveError.compressedRPGMakerGame.message)
    #expect(tree(games).allSatisfy { $0.hasPrefix(".importing") })
    #expect(tree(games.appendingPathComponent(".importing")).isEmpty)
}
