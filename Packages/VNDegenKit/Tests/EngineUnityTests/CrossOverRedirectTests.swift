#if os(macOS)
import Foundation
import TestSupport
import Testing
@testable import EngineUnity

/// Runs the player's real CrossOver against a scratch bottle VNDegen made, given in `VNDEGEN_TEST_BOTTLES` (its Bottles
/// folder). A CrossOver.conf that sets `CX_BOTTLE_PATH` to a decoy, as a player's may, must not redirect any command
/// VNDegen builds. CrossOver reads that file from `$HOME`, so the test gives it a scratch home and never touches the
/// player's.
@Suite(.enabled(if: ProcessInfo.processInfo.environment["VNDEGEN_TEST_BOTTLES"] != nil && CrossOver.installed() != nil))
struct CrossOverRedirectTests {
    @Test func aPlayersBottlePathCannotRedirectALaunch() throws {
        let fm = FileManager.default
        let folder = URL(fileURLWithPath: try #require(ProcessInfo.processInfo.environment["VNDEGEN_TEST_BOTTLES"]), isDirectory: true)
        let bottle = UnityBottle(folder: folder, tools: try #require(CrossOver.installed()).tools)
        #expect(fm.fileExists(atPath: bottle.path.appendingPathComponent("system.reg").path))

        let scratch = TestFolders.root.appendingPathComponent("redirect-\(UUID().uuidString)", isDirectory: true)
        defer { try? fm.removeItem(at: scratch) }
        let decoy = scratch.appendingPathComponent("decoy/\(UnityBottle.name)", isDirectory: true)
        try fm.createDirectory(at: decoy, withIntermediateDirectories: true)
        for name in ["system.reg", "user.reg", "userdef.reg"] { try Data().write(to: decoy.appendingPathComponent(name)) }
        try Data("[Bottle]\n\"Template\" = \"win10_64\"\n".utf8).write(to: decoy.appendingPathComponent("cxbottle.conf"))
        let config = scratch.appendingPathComponent("home/Library/Application Support/CrossOver", isDirectory: true)
        try fm.createDirectory(at: config, withIntermediateDirectories: true)
        try Data("[EnvironmentVariables]\n\"CX_BOTTLE_PATH\" = \"\(decoy.deletingLastPathComponent().path)\"\n".utf8)
            .write(to: config.appendingPathComponent("CrossOver.conf"))
        let before = try fm.contentsOfDirectory(atPath: decoy.path).sorted()

        var environment = ProcessInfo.processInfo.environment
        environment["HOME"] = scratch.appendingPathComponent("home").path
        environment["WINEPREFIX"] = decoy.path
        environment["CX_BOTTLE"] = UnityBottle.name
        let command = try bottle.command(.wine, ["--no-gui", "--ux-app", "/usr/bin/env"], environment: environment)
        let process = Process()
        process.executableURL = command.executable
        process.arguments = command.arguments
        process.environment = command.environment
        let output = Pipe()
        process.standardOutput = output
        process.standardError = FileHandle.nullDevice
        try process.run()
        let text = String(decoding: output.fileHandleForReading.readDataToEndOfFile(), as: UTF8.self)
        process.waitUntilExit()
        let prefix = text.split(separator: "\n").first { $0.hasPrefix("WINEPREFIX=") }.map { String($0.dropFirst("WINEPREFIX=".count)) }
        #expect(process.terminationStatus == 0)
        #expect(prefix == UnityBottle.resolved(bottle.path))
        #expect(try fm.contentsOfDirectory(atPath: decoy.path).sorted() == before)
    }
}
#endif
