import FontPack
import Foundation
import WebRuntime

extension GameFolder {
    /// Picks the web root the way `RPGMakerPlugin.detect` does: the top level when its js/ holds the engine core,
    /// otherwise www/.
    nonisolated init(rpgMakerGameAt root: URL) {
        let top = GameFolder(root: root, webRoot: "")
        let hasCore = { (dir: String) in
            ["rmmz_core.js", "rpg_core.js"].contains { top.resolve(virtualPath: "\(dir)/js/\($0)").map(Self.isFile) ?? false }
        }
        self = !hasCore(Self.virtualRoot) && hasCore("\(Self.virtualRoot)/www") ? GameFolder(root: root, webRoot: "www") : top
    }

    /// Where NW.js builds save: `save/` beside index.html.
    nonisolated var saveDirectory: URL? { resolve(virtualPath: "\(virtualWebRoot)/save") }

    /// True when the audio folder holds Ogg files, plain or encrypted (.rpgmvo), and no .m4a (.rpgmvm). MV asks for
    /// .m4a in a browser that cannot play Ogg, and its deployment usually ships both, but many games ship only Ogg. MV
    /// plays sounds only from these four folders. Listing each one also follows a folder that is a symlink.
    nonisolated var shipsOnlyOggAudio: Bool {
        let files = ["bgm", "bgs", "me", "se"].flatMap { folder in
            resolve(servedPath: "/audio/\(folder)").flatMap { try? FileManager.default.contentsOfDirectory(atPath: $0.path) } ?? []
        }
        var hasOgg = false
        for file in files {
            switch (file as NSString).pathExtension.lowercased() {
            case "m4a", "rpgmvm": return false
            case "ogg", "rpgmvo": hasOgg = true
            default: break
            }
        }
        return hasOgg
    }

    /// The names of the plugins `js/plugins.js` turns on.
    nonisolated var enabledPlugins: Set<String> {
        guard let data = read(servedPath: "/js/plugins.js", limit: 8 << 20) else { return [] }
        let text = String(decoding: data, as: UTF8.self)
        guard let start = text.firstIndex(of: "["), let end = text.lastIndex(of: "]"), start < end,
              let list = try? JSONSerialization.jsonObject(with: Data(text[start...end].utf8)) as? [[String: Any]]
        else { return [] }
        return Set(list.compactMap { $0["status"] as? Bool == true ? $0["name"] as? String : nil })
    }

    /// The Windows font names the game asks for in its plugins, plugin settings, and database, whose substitutes the
    /// page loads before the boot scene ends. MV's core asks for SimHei in Chinese games and Dotum in Korean ones.
    nonisolated var fontNames: Set<String> {
        let limit = 8 << 20
        var files = ["js/plugins.js", "data/System.json"]
        if let plugins = resolve(servedPath: "/js/plugins"),
           let names = try? FileManager.default.contentsOfDirectory(atPath: plugins.path) {
            files += names.filter { $0.lowercased().hasSuffix(".js") }.map { "js/plugins/\($0)" }
        }
        let texts = files.compactMap { read(servedPath: "/" + $0, limit: limit) }
        var found = FontPack.referencedNames(in: texts.map { String(decoding: $0, as: UTF8.self) }.joined(separator: "\n"))
        let system = read(servedPath: "/data/System.json", limit: limit)
            .flatMap { try? JSONSerialization.jsonObject(with: $0) as? [String: Any] }
        let locale = (system?["locale"] as? String)?.lowercased() ?? ""
        let installed = Set(FontPack.entries.map(\.name))
        if locale.hasPrefix("zh"), installed.contains("SimHei") { found.insert("SimHei") }
        if locale.hasPrefix("ko"), installed.contains("Dotum") { found.insert("Dotum") }
        return found
    }
}
