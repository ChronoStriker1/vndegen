import Foundation
import TestSupport
import Testing
@testable import WebRuntime

private struct Sandbox {
    let root: URL
    let bridge: NodeBridge

    init() throws {
        root = TestFolders.root.appendingPathComponent("vndegen-bridge-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        bridge = NodeBridge(folder: GameFolder(root: root, webRoot: ""))
    }

    func remove() { try? FileManager.default.removeItem(at: root) }
}

private func decode(_ json: String) throws -> BridgeRequest {
    try JSONDecoder().decode(BridgeRequest.self, from: Data(json.utf8))
}

@Test func wireFormatDecodesEveryOperation() throws {
    #expect(try decode(#"{"op":"exists","path":"/game/save"}"#) == .exists(path: "/game/save"))
    #expect(try decode(#"{"op":"readFile","path":"/game/a","text":true}"#) == .readFile(path: "/game/a", text: true))
    #expect(try decode(#"{"op":"readFile","path":"/game/a"}"#) == .readFile(path: "/game/a", text: false))
    #expect(try decode(#"{"op":"writeFile","path":"/game/a","data":"x","text":true}"#) == .writeFile(path: "/game/a", data: "x", text: true))
    #expect(try decode(#"{"op":"mkdir","path":"/game/save/","recursive":true}"#) == .mkdir(path: "/game/save/", recursive: true))
    #expect(try decode(#"{"op":"rename","from":"/game/a","to":"/game/b"}"#) == .rename(from: "/game/a", to: "/game/b"))
    #expect(try decode(#"{"op":"rmdir","path":"/game/www/testDirectory"}"#) == .rmdir(path: "/game/www/testDirectory"))
    #expect(try decode(#"{"op":"openExternal","url":"https://example.com"}"#) == .openExternal(url: "https://example.com"))
    #expect(try decode(#"{"op":"quit"}"#) == .quit)
    #expect(throws: DecodingError.self) { try decode(#"{"op":"format","path":"/"}"#) }
}

@Test func writesOutsideTheGameFolderFail() throws {
    let sandbox = try Sandbox()
    defer { sandbox.remove() }
    let reply = sandbox.bridge.handle(.writeFile(path: "/game/../escaped.txt", data: "no", text: true))
    guard case .failure(let error) = reply else { Issue.record("expected a failure, got \(reply)"); return }
    #expect(error.code == "EACCES")
    #expect(!FileManager.default.fileExists(atPath: sandbox.root.deletingLastPathComponent().appendingPathComponent("escaped.txt").path))
    #expect(sandbox.bridge.handle(.exists(path: "/etc/passwd")) == .value(.bool(false)))
    #expect(sandbox.bridge.handle(.readFile(path: "/etc/passwd", text: true)) == .failure(BridgeError(code: "EACCES", message: "path is outside the game folder: /etc/passwd")))
}

@Test func saveFlowRoundTripsBinaryStringsLikeNode() throws {
    let sandbox = try Sandbox()
    defer { sandbox.remove() }
    let bridge = sandbox.bridge
    // MZ hands fs a string of chars 0...255 and Node writes it as UTF-8. The same string must come back.
    let zip = String((0..<256).map { Character(UnicodeScalar(UInt8($0))) })

    #expect(bridge.handle(.mkdir(path: "/game/save/", recursive: false)) == .value(.null))
    #expect(bridge.handle(.mkdir(path: "/game/save/", recursive: false)) == .failure(BridgeError(code: "EEXIST", message: "file already exists")))
    #expect(bridge.handle(.exists(path: "/game/save/file1.rmmzsave")) == .value(.bool(false)))
    #expect(bridge.handle(.writeFile(path: "/game/save/file1.rmmzsave", data: zip, text: true)) == .value(.null))
    #expect(bridge.handle(.exists(path: "/game/save/file1.rmmzsave")) == .value(.bool(true)))
    #expect(bridge.handle(.readFile(path: "/game/save/file1.rmmzsave", text: true)) == .value(.string(zip)))
    let onDisk = try Data(contentsOf: sandbox.root.appendingPathComponent("save/file1.rmmzsave"))
    #expect(onDisk == Data(zip.utf8))

    #expect(bridge.handle(.rename(from: "/game/save/file1.rmmzsave", to: "/game/save/file1.rmmzsave_")) == .value(.null))
    #expect(bridge.handle(.writeFile(path: "/game/save/file1.rmmzsave", data: "new", text: true)) == .value(.null))
    #expect(bridge.handle(.rename(from: "/game/save/file1.rmmzsave_", to: "/game/save/file1.rmmzsave")) == .value(.null))
    #expect(bridge.handle(.readFile(path: "/game/save/file1.rmmzsave", text: true)) == .value(.string(zip)))
    #expect(bridge.handle(.readdir(path: "/game/save")) == .value(.strings(["file1.rmmzsave"])))
    #expect(bridge.handle(.unlink(path: "/game/save/file1.rmmzsave")) == .value(.null))
    #expect(bridge.handle(.unlink(path: "/game/save/file1.rmmzsave")) == .failure(BridgeError(code: "ENOENT", message: "no such file or directory, '/game/save/file1.rmmzsave'")))
}

@Test func rmdirRemovesOnlyEmptyDirectoriesInsideTheGame() throws {
    let sandbox = try Sandbox()
    defer { sandbox.remove() }
    let bridge = sandbox.bridge
    #expect(bridge.handle(.mkdir(path: "/game/www/testDirectory/", recursive: true)) == .value(.null))
    #expect(bridge.handle(.rmdir(path: "/game/WWW/testdirectory")) == .value(.null))
    #expect(!FileManager.default.fileExists(atPath: sandbox.root.appendingPathComponent("www/testDirectory").path))
    #expect(bridge.handle(.rmdir(path: "/game/www/testDirectory")) == .failure(BridgeError(code: "ENOENT", message: "no such file or directory, '/game/www/testDirectory'")))

    #expect(bridge.handle(.mkdir(path: "/game/www/save", recursive: false)) == .value(.null))
    #expect(bridge.handle(.writeFile(path: "/game/www/save/file1.rpgsave", data: "s", text: true)) == .value(.null))
    #expect(bridge.handle(.rmdir(path: "/game/www/save")) == .failure(BridgeError(code: "ENOTEMPTY", message: String(cString: strerror(ENOTEMPTY)))))
    guard case .failure(let notDirectory) = bridge.handle(.rmdir(path: "/game/www/save/file1.rpgsave")) else { Issue.record("removed a file"); return }
    #expect(notDirectory.code == "ENOTDIR")
    #expect(FileManager.default.fileExists(atPath: sandbox.root.appendingPathComponent("www/save/file1.rpgsave").path))
    // The game folder itself, and anything above it, stays.
    for path in ["/game", "/game/", "/game/www/..", "/"] {
        guard case .failure(let error) = bridge.handle(.rmdir(path: path)) else { Issue.record("removed \(path)"); continue }
        #expect(error.code == "EACCES")
    }
    #expect(FileManager.default.fileExists(atPath: sandbox.root.path))
}

/// MV's community core (`rpg_managers.js` "community-1.3b") picks www/save/ only when it can make and remove a folder
/// there. This is its code, run through the shim against a real game folder.
@Test @MainActor func communityCoreSavesBesideIndexHtml() async throws {
    let root = TestFolders.root.appendingPathComponent("vndegen-community-\(UUID().uuidString)", isDirectory: true)
    try FileManager.default.createDirectory(at: root.appendingPathComponent("www"), withIntermediateDirectories: true)
    defer { try? FileManager.default.removeItem(at: root) }
    let folder = GameFolder(root: root, webRoot: "www")
    let bridge = NodeBridge(folder: folder)
    let page = try PageHarness(storage: false, folder: folder) { bridge.handle($0) }
    try await page.load("<html><body></body></html>")
    let directory = try await page.run("""
        const fs = require('fs'), path = require('path');
        const base = path.dirname(process.mainModule.filename);
        const testPath = path.join(base, 'testDirectory/');
        let canMake;
        try {
            fs.mkdirSync(testPath);
            fs.rmdirSync(testPath);
            canMake = true;
        } catch (e) {
            canMake = false;
        }
        const dir = canMake ? path.join(base, 'save/') : path.join(path.dirname(base), 'save/');
        if (!fs.existsSync(dir)) fs.mkdirSync(dir);
        fs.writeFileSync(dir + 'file1.rpgsave', 'saved');
        await new Promise((done) => fs.rmdir(path.join(base, 'save'), (e) => { step(e && e.code); done(); }));
        return dir + ' ' + reached;
        """) as? String
    #expect(directory == "/game/www/save/ ENOTEMPTY")
    #expect(try String(contentsOf: root.appendingPathComponent("www/save/file1.rpgsave"), encoding: .utf8) == "saved")
    #expect(try FileManager.default.contentsOfDirectory(atPath: root.appendingPathComponent("www").path) == ["save"])
}

@Test @MainActor func tyranoFiveTwoFindsItsElectronPreload() async throws {
    let root = TestFolders.root.appendingPathComponent("vndegen-studio-\(UUID().uuidString)", isDirectory: true)
    try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
    defer { try? FileManager.default.removeItem(at: root) }
    let folder = GameFolder(root: root, webRoot: "")
    let bridge = NodeBridge(folder: folder)
    let appPath = "/game/Game.app/Contents/Resources/app.asar"
    let page = try PageHarness(storage: false, folder: folder, runtime: .electron(appPath: appPath)) { bridge.handle($0) }
    try await page.load("<html><body></body></html>")
    // What index.html, kag.js and libs.js ask for at startup, then a save the way $.setStorageFile writes one.
    let answers = try await page.run("""
        window.process = window.studio_api.ipcRenderer.sendSync('getProcess');
        const fs = window.studio_api.fs;
        fs.writeFileSync('/game/Goon_sf.sav', 'saved');
        return [window.studio_api.ipcRenderer.sendSync('doubleCheck', {}), window.studio_api.ipcRenderer.sendSync('getAppPath', {}),
                process.platform, process.execPath, fs.existsSync('/game/Goon_sf.sav'), process.__dirname, process.env.HOME];
        """) as? [Any]
    #expect(answers?[0] as? Bool == true)
    #expect(answers?[1] as? String == appPath)
    #expect(answers?[2] as? String == "darwin")
    #expect(answers?[3] as? String == "/game/index.html")
    #expect(answers?[4] as? Bool == true)
    // $.getUnzipPath reads __dirname, and an .asar in it keeps the game from unpacking patches into its folder.
    #expect(answers?[5] as? String == appPath)
    #expect(answers?[6] as? String == "/game")
    #expect(try String(contentsOf: root.appendingPathComponent("Goon_sf.sav"), encoding: .utf8) == "saved")
}

@Test func binaryReadsAndWritesUseBase64() throws {
    let sandbox = try Sandbox()
    defer { sandbox.remove() }
    let bytes = Data([0, 1, 2, 255, 254, 10, 13])
    #expect(sandbox.bridge.handle(.writeFile(path: "/game/blob.bin", data: bytes.base64EncodedString(), text: false)) == .value(.null))
    #expect(try Data(contentsOf: sandbox.root.appendingPathComponent("blob.bin")) == bytes)
    #expect(sandbox.bridge.handle(.readFile(path: "/game/blob.bin", text: false)) == .value(.string(bytes.base64EncodedString())))
    #expect(sandbox.bridge.handle(.copyFile(from: "/game/blob.bin", to: "/game/copy.bin")) == .value(.null))
    guard case .value(.stat(let stat)) = sandbox.bridge.handle(.stat(path: "/game/copy.bin")) else { Issue.record("stat failed"); return }
    #expect(stat.isFile && !stat.isDirectory && stat.size == 7)
    guard case .value(.stat(let dir)) = sandbox.bridge.handle(.stat(path: "/game")) else { Issue.record("stat failed"); return }
    #expect(dir.isDirectory)
    #expect(sandbox.bridge.handle(.readFile(path: "/game", text: true)) == .failure(BridgeError(code: "EISDIR", message: "illegal operation on a directory")))
}

@Test func copyFileNeverDeletesWhatItCopiesOnto() throws {
    let sandbox = try Sandbox()
    defer { sandbox.remove() }
    let bridge = sandbox.bridge
    #expect(bridge.handle(.mkdir(path: "/game/save", recursive: false)) == .value(.null))
    #expect(bridge.handle(.writeFile(path: "/game/save/file1.rmmzsave", data: "one", text: true)) == .value(.null))
    #expect(bridge.handle(.writeFile(path: "/game/save/file2.rmmzsave", data: "two", text: true)) == .value(.null))

    let isDirectory = BridgeError(code: "EISDIR", message: "illegal operation on a directory")
    #expect(bridge.handle(.copyFile(from: "/game/save/file1.rmmzsave", to: "/game/save")) == .failure(isDirectory))
    #expect(bridge.handle(.copyFile(from: "/game/save/file1.rmmzsave", to: "/game")) == .failure(isDirectory))
    #expect(bridge.handle(.copyFile(from: "/game/save", to: "/game/backup")) == .failure(isDirectory))
    #expect(bridge.handle(.copyFile(from: "/game/save/file1.rmmzsave", to: "/game/save/./file1.rmmzsave")) == .value(.null))
    #expect(bridge.handle(.readFile(path: "/game/save/file1.rmmzsave", text: true)) == .value(.string("one")))

    #expect(bridge.handle(.copyFile(from: "/game/save/file1.rmmzsave", to: "/game/save/file2.rmmzsave")) == .value(.null))
    #expect(bridge.handle(.readFile(path: "/game/save/file2.rmmzsave", text: true)) == .value(.string("one")))
    #expect(bridge.handle(.copyFile(from: "/game/save/file1.rmmzsave", to: "/game/missing/file1.rmmzsave"))
            == .failure(BridgeError(code: "ENOENT", message: "no such file or directory, '/game/missing/file1.rmmzsave'")))
    let left = try FileManager.default.contentsOfDirectory(atPath: sandbox.root.appendingPathComponent("save").path).sorted()
    #expect(left == ["file1.rmmzsave", "file2.rmmzsave"])
}

@Test func repliesSerializeForTheShim() {
    #expect(BridgeReply.value(.bool(true)).json == #"{"value":true}"#)
    #expect(BridgeReply.value(.null).json == #"{"value":null}"#)
    #expect(BridgeReply.failure(BridgeError(code: "ENOENT", message: "gone")).json == #"{"error":{"code":"ENOENT","message":"gone"}}"#)
}

@Test func rangeReadsStopAtTheEndOfTheFile() throws {
    let sandbox = try Sandbox()
    defer { sandbox.remove() }
    let bridge = sandbox.bridge
    #expect(try decode(#"{"op":"read","path":"/game/a","position":4,"length":2}"#) == .read(path: "/game/a", position: 4, length: 2))
    try Data("0123456789".utf8).write(to: sandbox.root.appendingPathComponent("digits.txt"))
    #expect(bridge.handle(.read(path: "/game/digits.txt", position: 3, length: 4)) == .value(.string(Data("3456".utf8).base64EncodedString())))
    #expect(bridge.handle(.read(path: "/game/digits.txt", position: 8, length: 4)) == .value(.string(Data("89".utf8).base64EncodedString())))
    #expect(bridge.handle(.read(path: "/game/digits.txt", position: 20, length: 4)) == .value(.string("")))
    #expect(bridge.handle(.read(path: "/game", position: 0, length: 1)) == .failure(BridgeError(code: "EISDIR", message: "illegal operation on a directory")))
    guard case .failure(let outside) = bridge.handle(.read(path: "/etc/passwd", position: 0, length: 1)) else { Issue.record("read outside"); return }
    #expect(outside.code == "EACCES")
}

/// Princess and Conquest's Club_VideoDecrypt reads each movie's first four bytes with openSync, readSync and closeSync.
/// The expected values are what Node 26's fs returned for the same calls on the same 20-byte file.
@Test @MainActor func descriptorReadsBehaveAsNode() async throws {
    let root = TestFolders.root.appendingPathComponent("vndegen-fd-\(UUID().uuidString)", isDirectory: true)
    try FileManager.default.createDirectory(at: root.appendingPathComponent("movies"), withIntermediateDirectories: true)
    defer { try? FileManager.default.removeItem(at: root) }
    try (Data([0x1a, 0x45, 0xdf, 0xa3]) + Data("ABCDEFGHIJKLMNOP".utf8)).write(to: root.appendingPathComponent("movies/clip.webm"))
    let folder = GameFolder(root: root, webRoot: "")
    let bridge = NodeBridge(folder: folder)
    let page = try PageHarness(storage: false, folder: folder) { bridge.handle($0) }
    try await page.load("<html><body></body></html>")
    let lines = try await page.run("""
        const fs = require('fs');
        const out = [];
        const rec = (label, f) => {
            try { out.push(label + ' = ' + JSON.stringify(f())); } catch (e) { out.push(label + ' ! ' + e.constructor.name + ' ' + e.code + ' ' + (e.syscall || '')); }
        };
        const fd = fs.openSync('movies/clip.webm', 'r');
        const head = Buffer.alloc(8);
        rec('positional', () => [fs.readSync(fd, head, 0, 4, 0), Array.from(head)]);
        rec('current 1', () => [fs.readSync(fd, head, 0, 3, null), Array.from(head)]);
        rec('current 2', () => [fs.readSync(fd, head, 2, 3, -1), Array.from(head)]);
        rec('positional keeps', () => [fs.readSync(fd, head, 0, 2, 10), fs.readSync(fd, head, 0, 2, null), Array.from(head)]);
        rec('bigint', () => [fs.readSync(fd, head, 0, 1, 5n), head[0]]);
        rec('options', () => [fs.readSync(fd, head, { offset: 1, length: 2, position: 1 }), Array.from(head)]);
        rec('three args', () => [fs.readSync(fd, head, { position: 0 }), Array.from(head)]);
        rec('past end', () => fs.readSync(fd, head, 0, 4, 100000));
        rec('near end', () => fs.readSync(fd, head, 0, 8, 14));
        rec('zero length', () => fs.readSync(fd, head, 0, 0, 0));
        rec('length too long', () => fs.readSync(fd, head, 4, 5, 0));
        rec('not a buffer', () => fs.readSync(fd, 'abc', 0, 1, 0));
        rec('typed', () => { const a = new Uint16Array(2); fs.readSync(fd, a, 1, 2, 0); return Array.from(new Uint8Array(a.buffer)); });
        rec('close', () => fs.closeSync(fd));
        rec('read closed', () => fs.readSync(fd, head, 0, 1, 0));
        rec('close closed', () => fs.closeSync(fd));
        rec('missing', () => fs.openSync('movies/none.webm', 'r'));
        const dir = fs.openSync('movies', 'r');
        rec('dir read', () => fs.readSync(dir, head, 0, 1, 0));
        fs.closeSync(dir);
        return out;
        """) as? [String]
    #expect(lines == [
        "positional = [4,[26,69,223,163,0,0,0,0]]",
        "current 1 = [3,[26,69,223,163,0,0,0,0]]",
        "current 2 = [3,[26,69,163,65,66,0,0,0]]",
        "positional keeps = [2,2,[67,68,163,65,66,0,0,0]]",
        "bigint = [1,66]",
        "options = [2,[66,69,223,65,66,0,0,0]]",
        "three args = [8,[26,69,223,163,65,66,67,68]]",
        "past end = 0",
        "near end = 6",
        "zero length = 0",
        "length too long ! RangeError ERR_OUT_OF_RANGE ",
        "not a buffer ! TypeError ERR_INVALID_ARG_TYPE ",
        "typed = [0,26,69,0]",
        "close = undefined",
        "read closed ! Error EBADF read",
        "close closed ! Error EBADF close",
        "missing ! Error ENOENT open",
        "dir read ! Error EISDIR read",
    ])
}
