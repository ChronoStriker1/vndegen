// The runtime package store. See RuntimePackage in VNCore.
import CLzmaDec
import CryptoKit
import Foundation
import Synchronization
import Testing
import VNCore
@testable import RuntimePackages

@Suite struct RuntimePackageStoreTests {
    @Test func refusesAChecksumMismatchAndLeavesNoFiles() async throws {
        let bytes = Data(repeating: 7, count: 300_000)
        let (session, url) = StubURLProtocol.serve(.init(body: .data(bytes)))
        let package = testPackage(url: url, sha256: String(repeating: "0", count: 64), size: Int64(bytes.count))
        let store = try TemporaryStore()
        await #expect(throws: RuntimeInstallError.checksumMismatch(expected: package.sha256, actual: sha256(bytes))) {
            try await store.store.install(package, session: session, progress: { _ in }, acceptLicense: { _ in true })
        }
        #expect(store.files().isEmpty)
        #expect(store.store.installedFolder(for: package) == nil)
    }

    @Test func refusesAWrongSizeAndAnHTTPError() async throws {
        let bytes = Data(repeating: 1, count: 1000)
        let store = try TemporaryStore()
        let (session, url) = StubURLProtocol.serve(.init(body: .data(bytes)))
        await #expect(throws: RuntimeInstallError.sizeMismatch(expected: 999, actual: 1000)) {
            try await store.store.install(testPackage(url: url, sha256: sha256(bytes), size: 999), session: session,
                                          progress: { _ in }, acceptLicense: { _ in true })
        }
        let (missingSession, missing) = StubURLProtocol.serve(.init(status: 404, body: .data(Data())))
        await #expect(throws: RuntimeInstallError.httpStatus(404)) {
            try await store.store.install(testPackage(url: missing, sha256: sha256(bytes), size: 1000),
                                          session: missingSession, progress: { _ in }, acceptLicense: { _ in true })
        }
        #expect(store.files().isEmpty)
    }

    @Test func cancellingMidDownloadStopsPromptlyAndLeavesNoFiles() async throws {
        let (session, url) = StubURLProtocol.serve(.init(body: .slow(chunk: Data(repeating: 3, count: 65_536))))
        let package = testPackage(url: url, sha256: String(repeating: "0", count: 64), size: 100_000_000)
        let store = try TemporaryStore()
        let (downloading, signal) = AsyncStream.makeStream(of: Int64.self)
        let task = Task {
            try await store.store.install(package, session: session, progress: { progress in
                if case .downloading(let received, _) = progress, received > 200_000 { signal.yield(received) }
            }, acceptLicense: { _ in true })
        }
        for await _ in downloading { break }
        let cancelled = ContinuousClock.now
        task.cancel()
        let result = await task.result
        #expect(throws: CancellationError.self) { try result.get() }
        #expect(ContinuousClock.now - cancelled < .seconds(5))
        #expect(store.files().isEmpty)
    }

    @Test func decliningTheLicenseLeavesNoFiles() async throws {
        let installer = SyntheticInnoSetup.make(license: "Synthetic license terms.", files: [
            ("{app}\\Graphics\\one.bin", [1, 2, 3]),
        ])
        let (session, url) = StubURLProtocol.serve(.init(body: .data(installer)), name: "setup.exe")
        let package = testPackage(url: url, sha256: sha256(installer), size: Int64(installer.count))
        let store = try TemporaryStore()
        let shown = Mutex<String??>(nil)
        await #expect(throws: RuntimeInstallError.declinedLicense) {
            try await store.store.install(package, session: session, progress: { _ in }, acceptLicense: { text in
                shown.withLock { $0 = text }
                return false
            })
        }
        #expect(shown.withLock { $0 } == "Synthetic license terms.")
        #expect(store.files().isEmpty)
    }

    @Test func installsReplacesAndRemovesASyntheticPackage() async throws {
        let store = try TemporaryStore()
        let first = SyntheticInnoSetup.make(license: "", files: [
            ("{app}\\Audio\\BGM\\theme.mid", Array("first".utf8)),
            ("{app}\\Game.ico", Array(repeating: 9, count: 5000)),
            ("{tmp}\\helper.dll", [0]),
            ("{app}\\Fonts\\a?b.txt", Array("odd".utf8)),
        ], directories: ["{app}\\Graphics\\Pictures", "{app}"])
        let (session, url) = StubURLProtocol.serve(.init(body: .data(first)), name: "setup.exe")
        let package = testPackage(url: url, sha256: sha256(first), size: Int64(first.count))
        let reports = Mutex<[RuntimeInstallProgress]>([])
        let folder = try await store.store.install(package, session: session, progress: { progress in
            reports.withLock { $0.append(progress) }
        }, acceptLicense: { text in text == nil })
        #expect(store.store.installedFolder(for: package) == folder)
        #expect(folder.lastPathComponent == package.id)
        #expect(try tree(folder) == [
            "Audio/": nil, "Audio/BGM/": nil, "Audio/BGM/theme.mid": sha256(Data("first".utf8)),
            "Fonts/": nil, "Fonts/a$b.txt": sha256(Data("odd".utf8)),
            "Game.ico": sha256(Data(repeating: 9, count: 5000)), "Graphics/": nil, "Graphics/Pictures/": nil,
        ])
        let seen = reports.withLock { $0 }
        #expect(seen.contains(.downloading(received: Int64(first.count), total: Int64(first.count))))
        #expect(seen.contains(.verifying))
        #expect(seen.contains(.extracting(done: 3, total: 3)))
        #expect(seen.last == .installing)

        let second = SyntheticInnoSetup.make(license: "", files: [("{app}\\Game.ico", [4])])
        let (secondSession, secondURL) = StubURLProtocol.serve(.init(body: .data(second)), name: "setup.exe")
        let newer = RuntimePackage(id: package.id, name: package.name, download: secondURL, sha256: sha256(second),
                                   size: Int64(second.count), installer: .innoSetup(path: nil), licensePage: package.licensePage)
        // A failed reinstall keeps the installed package.
        await #expect(throws: RuntimeInstallError.declinedLicense) {
            try await store.store.install(newer, session: secondSession, progress: { _ in }, acceptLicense: { _ in false })
        }
        #expect(try tree(folder).count == 8)
        _ = try await store.store.install(newer, session: secondSession, progress: { _ in }, acceptLicense: { _ in true })
        #expect(try tree(folder) == ["Game.ico": sha256(Data([4]))])

        try store.store.remove(package)
        #expect(store.store.installedFolder(for: package) == nil)
        #expect(store.files().isEmpty)
    }

    @Test func importsAFolder() throws {
        let store = try TemporaryStore()
        let source = store.root.deletingLastPathComponent().appending(path: "source-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: source.appending(path: "Graphics"), withIntermediateDirectories: true)
        try Data("tiles".utf8).write(to: source.appending(path: "Graphics/Tile.png"))
        defer { try? FileManager.default.removeItem(at: source) }
        let package = testPackage(url: URL(string: "https://example.invalid/rtp.zip")!, sha256: "", size: 0)
        let folder = try store.store.importFolder(source, as: package)
        #expect(store.store.installedFolder(for: package) == folder)
        #expect(try tree(folder) == ["Graphics/": nil, "Graphics/Tile.png": sha256(Data("tiles".utf8))])
        #expect(FileManager.default.fileExists(atPath: source.appending(path: "Graphics/Tile.png").path))
    }
}

@Suite struct InnoSetupReaderTests {
    @Test func mapsAppDestinationsToRelativePaths() {
        #expect(InnoSetupInstaller.appRelativePath("{app}\\Graphics\\Titles\\001-Title01.jpg") == "Graphics/Titles/001-Title01.jpg")
        #expect(InnoSetupInstaller.appRelativePath("{APP}\\a\\.\\b\\..\\c.txt") == "a/c.txt")
        #expect(InnoSetupInstaller.appRelativePath("{app}\\{{x}\\{lang}\\y:z") == "{x}/lang/y$z")
        #expect(InnoSetupInstaller.appRelativePath("{app}\\..\\..\\escape.txt") == "escape.txt")
        #expect(InnoSetupInstaller.appRelativePath("{tmp}\\file.txt") == nil)
        #expect(InnoSetupInstaller.appRelativePath("{app}") == "")
    }

    @Test(arguments: ["zlib", "bzip2"])
    func decodesCompressedStreamsInSmallPieces(_ method: String) throws {
        let original = (0..<200_000).map { UInt8(truncatingIfNeeded: $0 * 7 / 13) }
        let compressed: [UInt8]
        let decoder: any Decompressor
        if method == "zlib" {
            var size = compressBound(uLong(original.count))
            var output = [UInt8](repeating: 0, count: Int(size))
            #expect(compress2(&output, &size, original, uLong(original.count), 9) == Z_OK)
            compressed = Array(output[0..<Int(size)])
            decoder = try ZlibDecoder()
        } else {
            var size = UInt32(original.count + original.count / 100 + 600)
            var output = [CChar](repeating: 0, count: Int(size))
            var input = original.map { CChar(bitPattern: $0) }
            #expect(BZ2_bzBuffToBuffCompress(&output, &size, &input, UInt32(input.count), 9, 0, 0) == BZ_OK)
            compressed = output[0..<Int(size)].map { UInt8(bitPattern: $0) }
            decoder = try BZip2Decoder()
        }
        var decoded: [UInt8] = []
        var position = 0
        var buffer = [UInt8](repeating: 0, count: 1000)
        var ended = false
        while !ended {
            let piece = min(compressed.count, position + 333)
            let result = try compressed.withUnsafeBytes { input in
                try buffer.withUnsafeMutableBytes {
                    try decoder.decode(UnsafeRawBufferPointer(rebasing: input[position..<piece]), into: $0)
                }
            }
            position += result.consumed
            decoded += buffer[0..<result.produced]
            ended = result.ended
        }
        #expect(decoded == original)
    }

    /// The 5.3.9 form also flips the high byte of negative addresses. The real RTPs exercise the earlier forms.
    @Test func undoesTheCallFilterAcrossBufferBoundaries() {
        let encoded: [UInt8] = [0x90, 0xe8, 0x10, 0, 0, 0, 0xe9, 0x02, 0, 0, 0, 0xe8, 0x01, 0x02, 0x03, 0x04, 0xe8, 0x07]
        let cases: [(flip: Bool, expected: [UInt8])] = [
            (false, [0x90, 0xe8, 0x0a, 0, 0, 0, 0xe9, 0xf7, 0xff, 0xff, 0]),
            (true, [0x90, 0xe8, 0x0a, 0, 0, 0, 0xe9, 0xf7, 0xff, 0xff, 0xff]),
        ]
        for (flip, expected) in cases {
            var whole = InnoExecutableDecoder(.v5200(flipHighByte: flip))
            var output = whole.process(encoded)
            output += whole.finish()
            #expect(output == expected + [0xe8, 0x01, 0x02, 0x03, 0x04, 0xe8, 0x07])
            var pieces = InnoExecutableDecoder(.v5200(flipHighByte: flip))
            #expect(encoded.flatMap { pieces.process([$0]) } + pieces.finish() == output)
        }
    }

    @Test func refusesFilesThatAreNotInstallers() throws {
        let file = FileManager.default.temporaryDirectory.appending(path: "not-setup-\(UUID().uuidString).exe")
        try Data(repeating: 0x4d, count: 4096).write(to: file)
        defer { try? FileManager.default.removeItem(at: file) }
        #expect(throws: RuntimeInstallError.unsupportedInstaller("not an Inno Setup installer")) {
            try InnoSetupInstaller(setup: file)
        }
    }
}

/// The official RPG Maker RTP installers, read from `VNDEGEN_RTP_DOWNLOADS` and compared with innoextract 1.9's
/// output in `VNDEGEN_RTP_REFERENCE`. Skipped when those aren't set, since the RTPs can't be redistributed.
@Suite(.enabled(if: RealRTP.downloads != nil && RealRTP.reference != nil, "set VNDEGEN_RTP_DOWNLOADS and VNDEGEN_RTP_REFERENCE"))
struct RealRTPInstallerTests {
    @Test(arguments: RealRTP.all)
    func extractsTheSameFilesAsInnoextract(_ rtp: RealRTP) throws {
        let work = FileManager.default.temporaryDirectory.appending(path: "rtp-extract-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: work) }
        let started = ContinuousClock.now
        let setup: URL
        if case .innoSetup(let path?) = rtp.package.installer {
            setup = try ZipInstaller.extract(rtp.download, setupPath: path, to: work.appending(path: "installer"))
        } else {
            setup = rtp.download
        }
        let installer = try InnoSetupInstaller(setup: setup)
        let output = work.appending(path: "files")
        try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
        try installer.extractAppFiles(to: output) { _, _ in }
        let elapsed = ContinuousClock.now - started
        let ours = try tree(output)
        let theirs = try tree(RealRTP.reference!.appending(path: rtp.package.id))
        print("\(rtp.package.id): Inno Setup \(installer.headers.version), \(ours.count) entries, extracted in \(elapsed), "
              + "license \(installer.licenseText.map { "\($0.count) characters" } ?? "none")")
        #expect(Set(ours.keys).symmetricDifference(theirs.keys).sorted() == [])
        #expect(ours.filter { theirs[$0.key] != $0.value }.keys.sorted() == [])
    }

    /// Files Inno Setup ran through its x86 call filter, wherever they install, must decode to their stored digest.
    @Test(arguments: RealRTP.all)
    func undoesTheCallFilter(_ rtp: RealRTP) throws {
        let work = FileManager.default.temporaryDirectory.appending(path: "rtp-filter-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: work) }
        let setup: URL
        if case .innoSetup(let path?) = rtp.package.installer {
            setup = try ZipInstaller.extract(rtp.download, setupPath: path, to: work.appending(path: "installer"))
        } else {
            setup = rtp.download
        }
        let installer = try InnoSetupInstaller(setup: setup)
        var filtered: [Int: [String]] = [:]
        for (index, entry) in installer.headers.dataEntries.enumerated() {
            if case .noFilter = entry.filter { continue }
            filtered[index] = ["entry-\(index).bin"]
        }
        print("\(rtp.package.id): \(filtered.count) call-filtered files")
        try FileManager.default.createDirectory(at: work.appending(path: "files"), withIntermediateDirectories: true)
        try installer.extract(filtered, to: work.appending(path: "files")) { _, _ in }
    }

    @Test(arguments: RealRTP.all)
    func installsEndToEnd(_ rtp: RealRTP) async throws {
        let store = try TemporaryStore()
        let session = StubURLProtocol.serve(.init(body: .file(rtp.download)), at: rtp.package.download)
        let started = ContinuousClock.now
        let downloads = Mutex(0)
        let folder = try await store.store.install(rtp.package, session: session, progress: { progress in
            if case .downloading = progress { downloads.withLock { $0 += 1 } }
        }, acceptLicense: { _ in true })
        print("\(rtp.package.id): installed in \(ContinuousClock.now - started), \(downloads.withLock { $0 }) download reports")
        #expect(store.store.installedFolder(for: rtp.package) == folder)
        #expect(downloads.withLock { $0 } <= 102)
        let theirs = try tree(RealRTP.reference!.appending(path: rtp.package.id))
        #expect(try tree(folder) == theirs)
        #expect(store.files(in: ".staging").isEmpty)
    }
}

/// Downloads the XP RTP from its official URL through URLSession.shared, as the app does. Opt in with
/// VNDEGEN_RTP_NETWORK=1.
@Test(.enabled(if: ProcessInfo.processInfo.environment["VNDEGEN_RTP_NETWORK"] == "1", "set VNDEGEN_RTP_NETWORK=1"))
func installsTheXPRTPFromItsOfficialURL() async throws {
    let store = try TemporaryStore()
    let rtp = RealRTP.all[2]
    let started = ContinuousClock.now
    let folder = try await store.store.install(rtp.package, progress: { _ in }, acceptLicense: { _ in true })
    print("\(rtp.package.id): downloaded and installed in \(ContinuousClock.now - started)")
    #expect(store.store.installedFolder(for: rtp.package) == folder)
    #expect(FileManager.default.fileExists(atPath: folder.appending(path: "Graphics/Titles/001-Title01.jpg").path))
}

struct RealRTP: Sendable, CustomTestStringConvertible {
    let package: RuntimePackage
    let fileName: String

    var download: URL { Self.downloads!.appending(path: fileName) }
    var testDescription: String { package.id }

    static let downloads = ProcessInfo.processInfo.environment["VNDEGEN_RTP_DOWNLOADS"].map { URL(filePath: $0) }
    static let reference = ProcessInfo.processInfo.environment["VNDEGEN_RTP_REFERENCE"].map { URL(filePath: $0) }

    static let all: [RealRTP] = [
        RealRTP(package: RuntimePackage(
            id: "RPGVXAce", name: "RPG Maker VX Ace RTP", download: URL(string: "https://assets.rpgmakerweb.com/RPGVXAce_RTP.zip")!,
            sha256: "7e93d0ead93a686218b7c671bf099ef42f09f536083bd0b2f0fa6423a39fc19b", size: 194_690_591,
            installer: .innoSetup(path: "RTP100/Setup.exe"), licensePage: licensePage), fileName: "RPGVXAce_RTP.zip"),
        RealRTP(package: RuntimePackage(
            id: "RPGVX", name: "RPG Maker VX RTP", download: URL(string: "https://assets.rpgmakerweb.com/vx_rtp102e.zip")!,
            sha256: "8c82c02c876391d9585934454a629748d71b421c4957ada1dff8dc4b013ce403", size: 36_797_892,
            installer: .innoSetup(path: "RPGVX_RTP/Setup.exe"), licensePage: licensePage), fileName: "vx_rtp102e.zip"),
        RealRTP(package: RuntimePackage(
            id: "Standard", name: "RPG Maker XP RTP", download: URL(string: "https://assets.rpgmakerweb.com/xp_rtp104e.exe")!,
            sha256: "b3bd20ad7f413b40ac233aafd2e061de1dc429c2eadb59d0b3157ba3c47f16b2", size: 22_994_937,
            installer: .innoSetup(path: nil), licensePage: licensePage), fileName: "xp_rtp104e.exe"),
    ]

    private static let licensePage = URL(string: "https://www.rpgmakerweb.com/run-time-package")!
}

// MARK: - Helpers

private func testPackage(url: URL, sha256: String, size: Int64) -> RuntimePackage {
    RuntimePackage(id: "TestRuntime", name: "Test Runtime", download: url, sha256: sha256, size: size,
                   installer: .innoSetup(path: nil), licensePage: URL(string: "https://example.invalid/license")!)
}

private func sha256(_ data: Data) -> String {
    SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined()
}

/// Every file and folder below `folder`: folders as `path/` with nil, files with their SHA-256. Hashes stream, since
/// the RTPs hold large files.
private func tree(_ folder: URL) throws -> [String: String?] {
    var result: [String: String?] = [:]
    let base = folder.resolvingSymlinksInPath().path + "/"
    guard let walker = FileManager.default.enumerator(at: folder, includingPropertiesForKeys: [.isDirectoryKey]) else {
        return result
    }
    for case let url as URL in walker {
        let path = String(url.resolvingSymlinksInPath().path.dropFirst(base.count))
        if try url.resourceValues(forKeys: [.isDirectoryKey]).isDirectory == true {
            result[path + "/"] = .some(nil)
        } else {
            let handle = try FileHandle(forReadingFrom: url)
            defer { try? handle.close() }
            var hash = SHA256()
            while let data = try handle.read(upToCount: 1 << 20), !data.isEmpty { hash.update(data: data) }
            result[path] = hash.finalize().map { String(format: "%02x", $0) }.joined()
        }
    }
    return result
}

/// A store in a fresh temporary folder, deleted when the test ends.
private final class TemporaryStore: Sendable {
    let root: URL
    let store: RuntimePackageStore

    init() throws {
        root = FileManager.default.temporaryDirectory.appending(path: "runtime-store-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        store = RuntimePackageStore(root: root)
    }

    deinit { try? FileManager.default.removeItem(at: root) }

    /// Regular files anywhere under the store, or under one of its folders.
    func files(in subfolder: String? = nil) -> [String] {
        let folder = subfolder.map { root.appending(path: $0) } ?? root
        let walker = FileManager.default.enumerator(at: folder, includingPropertiesForKeys: [.isRegularFileKey])
        return (walker?.allObjects as? [URL] ?? []).filter {
            (try? $0.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile) == true
        }.map(\.path)
    }
}

/// Serves stubbed responses to sessions it makes, keyed by URL.
final class StubURLProtocol: URLProtocol, @unchecked Sendable {
    enum Body: Sendable {
        case data(Data)
        case file(URL)
        /// Sends `chunk` every 50 ms until the client stops loading.
        case slow(chunk: Data)
    }

    struct Stub: Sendable {
        var status = 200
        var body: Body
    }

    private static let stubs = Mutex<[URL: Stub]>([:])
    private let stopped = Mutex(false)

    static func serve(_ stub: Stub, name: String = "download.zip") -> (URLSession, URL) {
        let url = URL(string: "https://stub.invalid/\(UUID().uuidString)/\(name)")!
        return (serve(stub, at: url), url)
    }

    static func serve(_ stub: Stub, at url: URL) -> URLSession {
        stubs.withLock { $0[url] = stub }
        let configuration = URLSessionConfiguration.ephemeral
        configuration.protocolClasses = [StubURLProtocol.self]
        return URLSession(configuration: configuration)
    }

    override class func canInit(with request: URLRequest) -> Bool { true }
    override class func canonicalRequest(for request: URLRequest) -> URLRequest { request }

    override func startLoading() {
        guard let url = request.url, let stub = Self.stubs.withLock({ $0[url] }) else {
            client?.urlProtocol(self, didFailWithError: URLError(.fileDoesNotExist))
            return
        }
        let response = HTTPURLResponse(url: url, statusCode: stub.status, httpVersion: "HTTP/1.1", headerFields: nil)!
        client?.urlProtocol(self, didReceive: response, cacheStoragePolicy: .notAllowed)
        DispatchQueue.global().async { [self] in
            switch stub.body {
            case .data(let data):
                if !data.isEmpty { client?.urlProtocol(self, didLoad: data) }
            case .file(let file):
                guard let handle = try? FileHandle(forReadingFrom: file) else { break }
                defer { try? handle.close() }
                while !stopped.withLock({ $0 }), let data = try? handle.read(upToCount: 1 << 20), !data.isEmpty {
                    client?.urlProtocol(self, didLoad: data)
                }
            case .slow(let chunk):
                for _ in 0..<2000 where !stopped.withLock({ $0 }) {
                    client?.urlProtocol(self, didLoad: chunk)
                    Thread.sleep(forTimeInterval: 0.05)
                }
            }
            if !stopped.withLock({ $0 }) { client?.urlProtocolDidFinishLoading(self) }
        }
    }

    override func stopLoading() {
        stopped.withLock { $0 = true }
    }
}

/// Builds small Inno Setup 5.1.10 installers with stored (uncompressed) file data, laid out as the reader expects.
enum SyntheticInnoSetup {
    static func make(license: String, files: [(destination: String, contents: [UInt8])], directories: [String] = []) -> Data {
        var header: [UInt8] = []
        header += strings(11)  // app name through default group name
        header += string("setup") + string(license) + strings(2)  // base filename, license, info before and after
        header += strings(7 + 1 + 4)  // uninstall settings through serial, compiled code, readme through modify path
        header += zeros(32)  // lead bytes
        header += [0, 0, 0, 0, 0, 0].flatMap(le32)  // languages, messages, permissions, types, components, tasks
        header += le32(UInt32(directories.count)) + le32(UInt32(files.count)) + le32(UInt32(files.count)) + zeros(7 * 4)
        header += zeros(20 + 8 + 4)  // Windows versions, back colors, image back color
        header += zeros(16 + 8 + 8) + le32(1)  // password, salt, extra disk space, slices per disk
        header += zeros(5) + [0] + zeros(2)  // modes, compression (stored), architectures
        header += zeros(6)  // 44 option flags
        for directory in directories {
            header += string(directory) + strings(6) + zeros(4 + 20 + 2) + [0]
        }
        for (index, file) in files.enumerated() {
            header += strings(1) + string(file.destination) + strings(1 + 6) + zeros(20)
            header += le32(UInt32(index)) + zeros(4 + 8 + 2) + zeros(4) + [0]
        }

        var entries: [UInt8] = []
        var data: [UInt8] = []
        for file in files {
            entries += le32(0) + le32(0) + le32(0) + le64(UInt64(data.count)) + le64(UInt64(file.contents.count))
            entries += le64(UInt64(files.reduce(0) { $0 + $1.contents.count }))
            entries += Array(Insecure.MD5.hash(data: file.contents)) + zeros(8 + 8) + [0]
            data += file.contents
        }

        let headerOffset = 0x40 + 40
        let version = Array("Inno Setup Setup Data (5.1.10)".utf8)
        let blocks = block(header) + block(entries)
        let dataOffset = headerOffset + 64 + blocks.count
        var table = Array("rDlPtS07".utf8) + [0x87] + Array("eVx".utf8) + zeros(16)
        table += le32(UInt32(headerOffset)) + le32(UInt32(dataOffset))
        table += le32(crc(table))
        var file = zeros(0x30) + le32(0x6f6e_6e49) + le32(0x40) + le32(~UInt32(0x40)) + zeros(4)
        file += table + version + zeros(64 - version.count) + blocks
        file += Array("zlb".utf8) + [0x1a] + data
        return Data(file)
    }

    private static func block(_ payload: [UInt8]) -> [UInt8] {
        var stored: [UInt8] = []
        for start in stride(from: 0, to: payload.count, by: 4096) {
            let piece = Array(payload[start..<min(start + 4096, payload.count)])
            stored += le32(crc(piece)) + piece
        }
        let info = le32(UInt32(stored.count)) + [0]
        return le32(crc(info)) + info + stored
    }

    private static func crc(_ bytes: [UInt8]) -> UInt32 { UInt32(crc32(0, bytes, uInt(bytes.count))) }
    private static func zeros(_ count: Int) -> [UInt8] { [UInt8](repeating: 0, count: count) }
    private static func le32(_ value: UInt32) -> [UInt8] { (0..<4).map { UInt8(truncatingIfNeeded: value >> ($0 * 8)) } }
    private static func le64(_ value: UInt64) -> [UInt8] { (0..<8).map { UInt8(truncatingIfNeeded: value >> ($0 * 8)) } }
    private static func string(_ text: String) -> [UInt8] { le32(UInt32(text.utf8.count)) + Array(text.utf8) }
    private static func strings(_ count: Int) -> [UInt8] { zeros(4 * count) }
}
