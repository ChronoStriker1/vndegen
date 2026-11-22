import Foundation
import Testing
import TestSupport
import VNCore
@testable import EngineSystemAoi

private func vfs(_ names: [String]) -> Data {
    func u16(_ v: Int) -> Data { withUnsafeBytes(of: UInt16(v).littleEndian) { Data($0) } }
    func u32(_ v: Int) -> Data { withUnsafeBytes(of: UInt32(v).littleEndian) { Data($0) } }
    var table: [UInt16] = [0]
    var starts: [Int] = []
    for name in names {
        starts.append(table.count)
        table += Array(name.utf16) + [0]
    }
    var out = Data("VF".utf8) + u16(0x200) + u16(names.count) + u16(23) + u32(0) + u32(0)
    for start in starts {
        out += u32(start) + Data(count: 19)
    }
    out += u32(table.count)
    for unit in table { out += u16(Int(unit)) }
    return out
}

private func folder(_ name: String) throws -> URL {
    let root = TestFolders.root.appending(path: "\(name)-\(UUID().uuidString)")
    try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
    return root
}

@Suite struct SystemAoiLayoutTests {
    @Test func findsTheScriptArchive() throws {
        let root = try folder("systemaoi-layout")
        try vfs(["ac0000.box", "init.box", "pro.box"]).write(to: root.appending(path: "box.vfs"))
        try Data("MZ".utf8).write(to: root.appending(path: "Bunny3.exe"))
        let layout = SystemAoiLayout(FolderProbe(scanning: root))
        #expect(layout?.root == "")
        #expect(layout?.evidence == "box.vfs")
        #expect(SystemAoiPlugin().detect(FolderProbe(scanning: root))?.engine == .systemAoi)
    }

    @Test func findsAGameOneFolderDown() throws {
        let root = try folder("systemaoi-nested")
        let inner = root.appending(path: "Bunny3")
        try FileManager.default.createDirectory(at: inner, withIntermediateDirectories: true)
        try vfs(["INIT.BOX", "PRO.BOX"]).write(to: inner.appending(path: "box.vfs"))
        let layout = SystemAoiLayout(FolderProbe(scanning: root))
        #expect(layout?.root == "Bunny3")
        #expect(layout?.url(in: root).lastPathComponent == "Bunny3")
    }

    @Test func needsTheStartingScripts() throws {
        let root = try folder("systemaoi-other")
        try vfs(["init.box", "bo0000.box"]).write(to: root.appending(path: "box.vfs"))
        #expect(SystemAoiLayout(FolderProbe(scanning: root)) == nil)
    }

    @Test func needsAVFArchive() throws {
        let root = try folder("systemaoi-notvf")
        try Data(repeating: 0x41, count: 64).write(to: root.appending(path: "box.vfs"))
        #expect(SystemAoiPlugin().detect(FolderProbe(scanning: root)) == nil)
    }
}

@Suite struct SystemAoiInputTests {
    @Test func controlHeldSkips() {
        var router = SystemAoiInputRouter()
        #expect(router.hostKey(hidUsage: 224, pressed: true) == [.skip(true)])
        #expect(router.hostKey(hidUsage: 224, pressed: false) == [.skip(false)])
        #expect(router.hostKey(hidUsage: 40, pressed: true) == [.key(0x0d, down: true)])
    }
}
