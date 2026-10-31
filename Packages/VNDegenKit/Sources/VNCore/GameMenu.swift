/// One entry of a game's own menu, as VNDegen's Game menu lists it: a command, a submenu, or a separator line.
public struct GameMenuItem: Identifiable, Hashable, Sendable {
    public enum Kind: Hashable, Sendable {
        case command
        case submenu([GameMenuItem])
        case separator
    }

    /// Stable while the game's menu doesn't change, for `EngineSession.chooseGameMenuItem(_:)`.
    public let id: String
    public let title: String
    public let kind: Kind
    /// Whether the game shows a check mark by it.
    public let isChecked: Bool
    /// Whether it is one of a group of choices that the game checks one at a time, such as text speeds.
    public let isRadio: Bool
    public let isEnabled: Bool

    public init(id: String, title: String, kind: Kind = .command, isChecked: Bool = false, isRadio: Bool = false,
                isEnabled: Bool = true) {
        self.id = id
        self.title = title
        self.kind = kind
        self.isChecked = isChecked
        self.isRadio = isRadio
        self.isEnabled = isEnabled
    }

    public static func separator(id: String) -> GameMenuItem { GameMenuItem(id: id, title: "", kind: .separator) }

    public var items: [GameMenuItem] {
        if case let .submenu(items) = kind { items } else { [] }
    }
}
