import Foundation
import WebRuntime

/// What a TyranoScript game folder holds: where its web root is, which desktop runtime its PC build ships in, and how it
/// saves. Read from the folder once, for detection and for each launch.
nonisolated struct TyranoGame: Equatable, Sendable {
    enum Packaging: Equatable, Sendable {
        /// index.html, tyrano/, and data/ with no desktop runtime. Saves go to localStorage.
        case browser
        /// NW.js, with the web root at the top or in a package.nw folder beside nw.exe.
        case nwjs
        /// Electron, with the web root in resources/app/ or resources/app.asar.
        case electron
    }

    let webRoot: String
    let packaging: Packaging
    /// From kag.js, such as "4.50" or "5.20".
    let version: String?
    /// `projectID` in data/system/Config.tjs, or Tyrano's default. Every save file and localStorage key starts with it.
    let projectID: String
    /// `configSave=file`: saves are `<projectID>_*.sav` files beside the PC build's executable, which is the game folder.
    /// Otherwise they go to localStorage.
    let savesToFiles: Bool

    static let engineFile = "tyrano/plugins/kag/kag.js"
    /// Where Tyrano exports put index.html, most common first.
    static let webRoots = ["", "resources/app", "resources/app.asar", "package.nw"]
}

nonisolated extension TyranoGame {
    init?(folder: URL) {
        guard let (webRoot, web) = Self.webRoots.lazy
            .map({ ($0, GameFolder(root: folder, webRoot: $0)) })
            .first(where: { $0.1.read(servedPath: "/" + Self.engineFile, limit: 1) != nil })
        else { return nil }
        self.webRoot = webRoot
        packaging = webRoot.hasPrefix("resources/") ? .electron
            : web.read(servedPath: "/package.json", limit: 1) != nil ? .nwjs : .browser
        version = web.read(servedPath: "/" + Self.engineFile, limit: 4096).flatMap { Self.version(kagScript: String(decoding: $0, as: UTF8.self)) }
        let text = web.read(servedPath: "/data/system/Config.tjs", limit: 1 << 20).map { String(decoding: $0, as: UTF8.self) } ?? ""
        let config = Self.config(text)
        projectID = config["projectID"] ?? "tyranoproject"
        savesToFiles = packaging != .browser && config["configSave"] == "file"
    }

    /// kag.js opens with `tyrano.plugin.kag={version:450,` where 450 means 4.50.
    static func version(kagScript text: String) -> String? {
        guard let match = text.firstMatch(of: /version\s*:\s*(\d{3,4})\b/), let number = Int(match.1) else { return nil }
        return "\(number / 100).\(String(format: "%02d", number % 100))"
    }

    /// Config.tjs as Tyrano's `compileConfig` in kag.parser.js reads it: lines that start with `;` after trimming, cut at
    /// the first `//`, with every `;` and `"` removed, split at `=`. The key is what comes before the first `=` and the
    /// value what lies between it and the next one, both trimmed. A later line for the same key wins.
    static func config(_ text: String) -> [String: String] {
        // jQuery's trim: JavaScript white space and line terminators, which include the byte order mark.
        let space = CharacterSet.whitespacesAndNewlines.union(CharacterSet(charactersIn: "\u{FEFF}"))
        var settings: [String: String] = [:]
        for rawLine in text.components(separatedBy: "\n") {
            var line = rawLine.trimmingCharacters(in: space)
            guard line.hasPrefix(";") else { continue }
            if let comment = line.range(of: "//") { line = line[..<comment.lowerBound].trimmingCharacters(in: space) }
            line = line.replacingOccurrences(of: ";", with: "").replacingOccurrences(of: "\"", with: "")
            let parts = line.components(separatedBy: "=")
            settings[parts[0].trimmingCharacters(in: space)] = parts.count > 1 ? parts[1].trimmingCharacters(in: space) : ""
        }
        return settings
    }
}
