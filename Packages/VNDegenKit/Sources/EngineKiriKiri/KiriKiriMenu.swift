import Foundation
import VNCore

/// A KiriKiri game's window menu bar, which the engine never draws, as VNDegen's Game commands list it.
nonisolated enum KiriKiriMenu {
    /// One item as `vnkk_menu` describes it.
    private struct Item: Decodable {
        var id: String
        var title: String
        var checked: Bool
        var radio: Bool
        var enabled: Bool
        var items: [Item]
    }

    /// The Game commands' items from `vnkk_menu`'s JSON. Separators that would start or end a list, or follow another,
    /// are left out, and so are submenus left with nothing to choose.
    static func items(json: String) -> [GameMenuItem] {
        guard let items = try? JSONDecoder().decode([Item].self, from: Data(json.utf8)) else { return [] }
        return menu(items)
    }

    private static func menu(_ items: [Item]) -> [GameMenuItem] {
        var result: [GameMenuItem] = []
        for item in items {
            if item.title == "-" {
                if let last = result.last, last.kind != .separator { result.append(.separator(id: item.id)) }
                continue
            }
            let title = caption(item.title)
            if item.items.isEmpty {
                result.append(GameMenuItem(id: item.id, title: title, isChecked: item.checked, isRadio: item.radio,
                                           isEnabled: item.enabled))
            } else {
                let children = menu(item.items)
                guard !children.isEmpty else { continue }
                result.append(GameMenuItem(id: item.id, title: title, kind: .submenu(children), isEnabled: item.enabled))
            }
        }
        if result.last?.kind == .separator { result.removeLast() }
        return result
    }

    /// A Windows menu caption as a label: without its access key, such as the `(&S)` of `System(&S)` or the `&` of
    /// `&Save`, and without the shortcut after a tab, such as `Console(&C)\tShift+F4`.
    static func caption(_ caption: String) -> String {
        var text = Substring(caption.prefix { $0 != "\t" })
        if let key = text.range(of: #"\s*\(&[^)]\)"#, options: .regularExpression) { text.removeSubrange(key) }
        var result = ""
        var iterator = text.makeIterator()
        while let character = iterator.next() {
            guard character == "&" else {
                result.append(character)
                continue
            }
            // "&&" is a literal ampersand. A single one marks the next character as the access key.
            if let next = iterator.next() { result.append(next) }
        }
        return result.trimmingCharacters(in: .whitespaces)
    }
}

/// What a KiriKiri game is told when it asks whether its window menu bar is visible, as the game page's advanced
/// option sets it.
nonisolated enum KiriKiriMenuBar: String, Codable, CaseIterable {
    /// The game puts its menu commands on screen, if it has a way to.
    case hidden
    /// The game builds its menu bar, and VNDegen lists it in Game commands.
    case shown

    var title: String {
        switch self {
        case .hidden: "Commands on screen"
        case .shown: "In Game commands"
        }
    }

    /// What the game page says automatic picked.
    var automaticTitle: String {
        switch self {
        case .hidden: "commands on screen, as the game's scripts expect"
        case .shown: "in Game commands, as the game's scripts expect"
        }
    }

    /// The `VNKK_MENU_BAR_` value for `vnkk_set_menu_bar`, automatic for nil.
    static func engineMode(_ choice: KiriKiriMenuBar?) -> Int32 {
        switch choice {
        case nil: 0
        case .hidden: 1
        case .shown: 2
        }
    }
}

/// The player's choice for the menu bar and what automatic picked the last time the game asked, in the game's data
/// directory.
nonisolated struct KiriKiriMenuBarSettings: Codable, Equatable {
    var choice: KiriKiriMenuBar?
    var automatic: KiriKiriMenuBar?

    static func file(_ dataDirectory: URL) -> URL { dataDirectory.appendingPathComponent("menu-bar.json") }

    static func load(_ dataDirectory: URL) -> KiriKiriMenuBarSettings {
        guard let data = try? Data(contentsOf: file(dataDirectory)) else { return KiriKiriMenuBarSettings() }
        return (try? JSONDecoder().decode(KiriKiriMenuBarSettings.self, from: data)) ?? KiriKiriMenuBarSettings()
    }

    func save(_ dataDirectory: URL) throws {
        try FileManager.default.createDirectory(at: dataDirectory, withIntermediateDirectories: true)
        try JSONEncoder().encode(self).write(to: Self.file(dataDirectory), options: .atomic)
    }
}

extension KiriKiriPlugin {
    /// Whether the game's window menu bar reports visible, for games that change what they show on screen by it.
    nonisolated public func variants(for game: Game, dataDirectory: URL) -> EngineVariants? {
        let settings = KiriKiriMenuBarSettings.load(dataDirectory)
        return EngineVariants(
            title: "Window menu",
            options: KiriKiriMenuBar.allCases.map { .init(id: $0.rawValue, title: $0.title) },
            selected: settings.choice?.rawValue,
            automatic: settings.automatic?.automaticTitle ?? "picked from the game's scripts when it asks")
    }

    nonisolated public func selectVariant(_ id: String?, for game: Game, dataDirectory: URL) throws {
        var settings = KiriKiriMenuBarSettings.load(dataDirectory)
        settings.choice = id.flatMap(KiriKiriMenuBar.init(rawValue:))
        try settings.save(dataDirectory)
    }
}
