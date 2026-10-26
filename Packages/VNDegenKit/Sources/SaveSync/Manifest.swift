import Foundation

/// What VNDegen keeps from Ludusavi's primary manifest for one game.
public struct ManifestGame: Codable, Equatable, Sendable {
    /// The Steam app ID first, then the extra IDs the same game has.
    public var steamIDs: [Int] = []
    /// Folder names the game installs into, such as `NoSocksRPG` under `steamapps/common`.
    public var installDirs: [String] = []
    /// File rules tagged as saves or not tagged, such as `<base>/save/*.rmmzsave`.
    public var saveRules: [String] = []
    /// The title this entry is another name for.
    public var alias: String?

    public init(steamIDs: [Int] = [], installDirs: [String] = [], saveRules: [String] = [], alias: String? = nil) {
        self.steamIDs = steamIDs
        self.installDirs = installDirs
        self.saveRules = saveRules
        self.alias = alias
    }
}

/// Ludusavi's manifest by title, reduced to what matching and path mapping use, with lookup tables built once when the
/// manifest is read.
public struct ManifestIndex: Codable, Sendable {
    public private(set) var games: [String: ManifestGame] = [:]
    private var bySteamID: [Int: String] = [:]
    private var byNormalizedTitle: [String: String] = [:]
    private var byLetters: [String: String] = [:]

    public init(games: [String: ManifestGame]) {
        self.games = games
        // Sorted so the same manifest always gives the same answer when two titles collide.
        for title in games.keys.sorted() {
            for (offset, id) in games[title]!.steamIDs.enumerated() where offset == 0 || bySteamID[id] == nil { bySteamID[id] = title }
            byNormalizedTitle[LudusaviTitle.normalize(title)] = byNormalizedTitle[LudusaviTitle.normalize(title)] ?? title
            byLetters[LudusaviTitle.letters(title)] = byLetters[LudusaviTitle.letters(title)] ?? title
        }
    }

    public func game(_ title: String) -> ManifestGame? { games[title] }

    /// The title Ludusavi files the game under. An alias resolves to the title it stands for.
    func canonical(_ title: String) -> String { games[title]?.alias ?? title }

    public func title(steamID: Int) -> String? { bySteamID[steamID].map(canonical) }

    public func title(matching name: String) -> String? {
        (byNormalizedTitle[LudusaviTitle.normalize(name)] ?? byLetters[LudusaviTitle.letters(name)]).map(canonical)
    }

    /// Reads the manifest one line at a time. The file is machine-written YAML with two-space indents, so each line's
    /// indent says what it belongs to, and a full YAML tree of its 50,000 games is never built.
    public init(scanning text: String) {
        var games: [String: ManifestGame] = [:]
        var title: String?
        var game = ManifestGame()
        var section = "", subsection = ""
        var rule: String?
        var ruleTags: [String] = []

        func finishRule() {
            if let rule, ruleTags.isEmpty || ruleTags.contains("save") { game.saveRules.append(rule) }
            rule = nil
            ruleTags = []
        }
        func finishGame() {
            finishRule()
            if let title { games[title] = game }
        }

        for raw in text.split(separator: "\n") {
            let line = raw.last == "\r" ? raw.dropLast() : raw
            let body = line.drop { $0 == " " }
            guard !body.isEmpty, !body.hasPrefix("#"), body != "---" else { continue }
            let item = body.hasPrefix("- ") ? YAML.scalarText(body.dropFirst(2)) : nil
            let entry = item == nil ? YAML.splitKey(body) : nil
            switch line.count - body.count {
            case 0:
                finishGame()
                title = entry?.key
                game = ManifestGame()
                section = ""
            case 2:
                finishRule()
                section = entry?.key ?? ""
                if section == "alias", let rest = entry?.rest { game.alias = YAML.scalarText(rest) }
            case 4:
                guard let entry else { continue }
                switch section {
                case "steam": if entry.key == "id", let id = YAML.scalarText(entry.rest).flatMap({ Int($0) }) { game.steamIDs.insert(id, at: 0) }
                case "installDir": game.installDirs.append(entry.key)
                case "files":
                    finishRule()
                    rule = entry.key
                case "id": subsection = entry.key
                default: break
                }
            case 6:
                if section == "id", subsection == "steamExtra", let id = item.flatMap({ Int($0) }) { game.steamIDs.append(id) }
                if section == "files", let entry { subsection = entry.key }
            case 8:
                if section == "files", subsection == "tags", let item { ruleTags.append(item) }
            default: break
            }
        }
        finishGame()
        self.init(games: games)
    }

}

/// Ludusavi's title rules (src/scan/title.rs).
public enum LudusaviTitle {
    /// `normalize_title`: lowercase, without a year suffix, an edition suffix, symbols, or repeated spaces.
    public static func normalize(_ title: String) -> String {
        var text = title.lowercased()
        text = text.replacing(#/ \(\d+\)$/#, with: "")
        text = text.replacing(/[™®©:-] .+ edition$/, with: "")
        text = text.replacing(#/ (game of the year) edition$/#, with: "")
        text = text.replacing(#/ [^ ]+ edition$/#, with: "")
        text = text.replacing(/[™®©:-]/, with: " ")
        text = text.replacing(/['"‘’“”]/, with: "")
        text = text.replacing(#/ {2,}/#, with: " ")
        return text.trimmingCharacters(in: .whitespaces)
    }

    /// Letters and digits only, so `NoSocksRPG`, a typical install folder name, matches `No Socks RPG`.
    public static func letters(_ title: String) -> String {
        String(title.lowercased().unicodeScalars.filter(CharacterSet.alphanumerics.contains).map(Character.init))
    }
}
