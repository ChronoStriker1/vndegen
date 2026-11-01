import Foundation
import VNCore

/// The NW.js and Electron files PC builds of web games ship around the game. VNDegen runs the game's pages in WebKit and
/// opens none of these. Names come from the library's RPG Maker MV and MZ and TyranoScript games.
nonisolated public enum DesktopRuntimeFiles {
    /// Shared by both runtimes, which are Chromium builds.
    private static let chromium: Set<String> = [
        "d3dcompiler_47.dll", "dxcompiler.dll", "dxil.dll", "ffmpeg.dll", "icudtl.dat", "libEGL.dll", "libGLESv2.dll",
        "natives_blob.bin", "resources.pak", "snapshot_blob.bin", "swiftshader/libEGL.dll", "swiftshader/libGLESv2.dll",
        "v8_context_snapshot.bin", "vk_swiftshader.dll", "vk_swiftshader_icd.json", "vulkan-1.dll",
    ]

    /// Only `.pak` files, because a game can keep its own translations in `locales`, as a library MZ game does.
    private static let locales = ["locales": [".pak", ".pak.info"]]

    /// An `.exe` this big beside a runtime's files is that runtime under the game's name. A launcher stub or a tool is far
    /// smaller. In the library renamed runtimes are 46 to 162 MB, and tools at most 27 MB.
    static let renamedRuntimeSize: Int64 = 30_000_000

    /// NW.js's `credits.html` is 870 KB to 9 MB in the library. A smaller one is the game's own page.
    static let creditsSize: Int64 = 500_000

    /// NW.js's files. Nil without `nw.dll` or `nw.pak`, which show the other names here really are NW.js's.
    public static func nwjs(_ probe: FolderProbe) -> EngineFileList? {
        guard probe.contains("nw.dll") || probe.contains("nw.pak") else { return nil }
        return EngineFileList(
            files: withRenamedRuntime(chromium.union([
                "Game.exe", "nw.exe", "nw.dll", "node.dll", "ffmpegsumo.dll", "pdf.dll", "nw_elf.dll", "nwjc.exe", "chromedriver.exe",
                "payload.exe", "notification_helper.exe", "nacl_irt_x86_64.nexe", "nw.pak", "nw_100_percent.pak", "nw_200_percent.pak",
            ]), probe),
            filesOfAtLeast: ["credits.html": creditsSize],
            fileEndings: locales.merging(["pnacl": ["_nexe"]]) { a, _ in a },
            // NW.js's own Mac build holds nothing else. An `app.nw` in it could be the game.
            folders: FileManager.default.fileExists(atPath: FileCase.resolve("nwjs.app/Contents/Resources/app.nw", in: probe.root).path) ? [] : ["nwjs.app"])
    }

    /// Electron's files. Nil unless `icudtl.dat` and the game's `resources/app` or `app.asar` are there. The game stays.
    public static func electron(_ probe: FolderProbe) -> EngineFileList? {
        guard probe.contains("icudtl.dat"), probe.contains("resources/app") || probe.contains("resources/app.asar") else { return nil }
        return EngineFileList(
            files: withRenamedRuntime(chromium.union([
                "chrome_100_percent.pak", "chrome_200_percent.pak", "LICENSES.chromium.html", "resources/electron.asar", "resources/default_app.asar",
            ]), probe),
            fileEndings: locales)
    }

    /// Adds the one large `.exe` beside the runtime's files. With two, VNDegen can't tell which is the runtime and which
    /// a game or a translation brought, so neither is listed, even under a name on the list. With one not downloaded yet,
    /// iCloud's placeholder hides it from this count, so no `.exe` is listed at all.
    private static func withRenamedRuntime(_ names: Set<String>, _ probe: FolderProbe) -> Set<String> {
        let hidden = (try? FileManager.default.contentsOfDirectory(atPath: probe.root.path)) ?? []
        if hidden.contains(where: { $0.hasPrefix(".") && $0.lowercased().hasSuffix(".exe.icloud") }) {
            return Set(names.map { $0.lowercased() }.filter { !$0.hasSuffix(".exe") })
        }
        let large = probe.entries.filter { !$0.contains("/") && $0.lowercased().hasSuffix(".exe") && (probe.size($0) ?? 0) >= renamedRuntimeSize }
        let lowered = Set(names.map { $0.lowercased() })
        return large.count == 1 ? lowered.union(large.map { $0.lowercased() }) : lowered.subtracting(large.map { $0.lowercased() })
    }
}
