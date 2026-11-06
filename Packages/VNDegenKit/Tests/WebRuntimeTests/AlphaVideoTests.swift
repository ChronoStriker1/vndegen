import CVNAlpha
import Foundation
import Testing

private let fixtures = URL(fileURLWithPath: #filePath).deletingLastPathComponent().appendingPathComponent("Fixtures")

/// Runs `body` with a descriptor for the fixture, opened with `flags`.
private func withFixture<T>(_ name: String, flags: Int32 = O_RDONLY, _ body: (Int32) throws -> T) throws -> T {
    let descriptor = open(fixtures.appendingPathComponent(name).path, flags)
    try #require(descriptor >= 0)
    defer { close(descriptor) }
    return try body(descriptor)
}

private func inspect(_ descriptor: Int32) -> (Int32, String) {
    var error = [CChar](repeating: 0, count: 512)
    let status = vnalpha_inspect(descriptor, &error, Int32(error.count))
    return (status, String(decoding: error.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self))
}

@Test func anAlphaClipIsToldApartFromAnOpaqueOne() throws {
    #expect(try withFixture("alpha-clip.webm") { inspect($0).0 } == 1)
    #expect(try withFixture("opaque-clip.webm") { inspect($0).0 } == 0)
}

/// A clip the app can't read is an error, not a clip without alpha. Reading it by reopening /dev/fd/<n> called it
/// opaque, so it was served as WebM and WebKit drew its transparent parts white (#44).
@Test func anUnreadableClipIsAnErrorNotAnOpaqueClip() throws {
    let (status, reason) = try withFixture("alpha-clip.webm", flags: O_WRONLY) { inspect($0) }
    #expect(status == -1)
    #expect(!reason.isEmpty)
}

@Test func theCheckLeavesTheDescriptorWhereItWas() throws {
    try withFixture("alpha-clip.webm") { descriptor in
        #expect(lseek(descriptor, 7, SEEK_SET) == 7)
        #expect(inspect(descriptor).0 == 1)
        #expect(lseek(descriptor, 0, SEEK_CUR) == 7)
    }
}

@Test func aClipConvertsFromItsDescriptor() throws {
    let folder = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
    try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
    defer { try? FileManager.default.removeItem(at: folder) }
    let output = folder.appendingPathComponent("clip.mov")
    var result = vnalpha_result(), error = [CChar](repeating: 0, count: 512)
    let status = try withFixture("alpha-clip.webm") {
        vnalpha_convert($0, output.path, nil, &result, &error, Int32(error.count))
    }
    #expect(status == 0)
    #expect(result.width == 64 && result.height == 64 && result.frames == 3)
    #expect(FileManager.default.fileExists(atPath: output.path))
}
