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
    /// `projectID` in data/system/Config.tjs. Every save file and localStorage key starts with it.
    let projectID: String?
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
        let config = web.read(servedPath: "/data/system/Config.tjs", limit: 1 << 20).map { String(decoding: $0, as: UTF8.self) } ?? ""
        projectID = Self.setting("projectID", in: config)
        savesToFiles = packaging != .browser && Self.setting("configSave", in: config) == "file"
    }

    /// kag.js opens with `tyrano.plugin.kag={version:450,` where 450 means 4.50.
    static func version(kagScript text: String) -> String? {
        guard let match = text.firstMatch(of: /version\s*:\s*(\d{3,4})\b/), let number = Int(match.1) else { return nil }
        return "\(number / 100).\(String(format: "%02d", number % 100))"
    }

    /// A Config.tjs line such as `;projectID = testgame01;` or `;configSave=file`.
    static func setting(_ name: String, in config: String) -> String? {
        for line in config.split(whereSeparator: \.isNewline) {
            let trimmed = line.trimmingCharacters(in: .whitespaces)
            guard trimmed.hasPrefix(";") else { continue }
            let parts = trimmed.dropFirst().split(separator: "=", maxSplits: 1)
            guard parts.count == 2, parts[0].trimmingCharacters(in: .whitespaces) == name else { continue }
            let value = parts[1].split(separator: ";").first?.trimmingCharacters(in: .whitespaces) ?? ""
            return value.isEmpty ? nil : value
        }
        return nil
    }
}
