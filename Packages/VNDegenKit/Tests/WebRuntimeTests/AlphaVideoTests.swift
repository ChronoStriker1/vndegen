import CVNAlpha
import Foundation
import OSLog
import TestSupport
import Testing
@testable import WebRuntime

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
    let folder = TestFolders.root.appendingPathComponent(UUID().uuidString)
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

/// Princess and Conquest decrypts its movies in the page and plays them from blob URLs, which never reach the scheme
/// handler. A WebM blob with alpha is kept by the app instead and served converted; one without alpha stays a blob.
@Test @MainActor func aWebMBlobWithAlphaIsServedConverted() async throws {
    let root = TestFolders.root.appending(path: "vndegen-alpha-blob-\(UUID().uuidString)")
    try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
    let key = "998b446bd8ed571a955cb38250026178a918d2a4857ea95e78803d2da2e89791"
    defer {
        try? FileManager.default.removeItem(at: root)
        try? FileManager.default.removeItem(at: URL.cachesDirectory.appending(path: "AlphaVideo-v1/\(key).mov"))
    }
    try Data("<html><body></body></html>".utf8).write(to: root.appending(path: "index.html"))
    for name in ["alpha-clip", "opaque-clip"] {
        try FileManager.default.copyItem(at: fixtures.appendingPathComponent("\(name).webm"), to: root.appending(path: "\(name).bin"))
    }
    let folder = GameFolder(root: root, webRoot: "")
    let handler = GameSchemeHandler(folder: folder, log: Logger(), alpha: true)
    let page = try PageHarness(storage: false, scripts: ["window.__vndegen.alphaVideo = (url) => { window.converted = url; };"],
                               scheme: handler, alphaVideo: true) { request in
        guard case .alphaVideo(let data) = request else { return .failure(BridgeError(code: "ENOSYS", message: "")) }
        return handler.stash(base64: data)
    }
    try await page.load(URLRequest(url: folder.indexURL))
    let values = try await page.run("""
        const blob = async (name) => new Blob([new Uint8Array(await (await fetch(name)).arrayBuffer())], { type: 'video/webm' });
        const alpha = URL.createObjectURL(await blob('alpha-clip.bin'));
        const opaque = URL.createObjectURL(await blob('opaque-clip.bin'));
        const response = await fetch(alpha);
        const movie = new Uint8Array(await response.arrayBuffer());
        return {
            alpha, opaque: opaque.slice(0, 5), type: response.headers.get('Content-Type'),
            atom: String.fromCharCode(...movie.slice(4, 8)), converted: window.converted,
        };
        """) as? [String: Any] ?? [:]
    let address = "vndegen://game/.vndegen/alpha/\(key).webm"
    #expect(values["alpha"] as? String == address)
    #expect(values["opaque"] as? String == "blob:")
    #expect(values["type"] as? String == "video/quicktime")
    #expect(values["atom"] as? String == "ftyp")
    #expect(values["converted"] as? String == address)
}
