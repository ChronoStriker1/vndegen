import Foundation
import VNCore

/// The window menu bar an NScripter game builds with `insertmenu`, which the runtime never draws, as VNDegen's Game
/// menu lists it.
nonisolated enum NScripterMenu {
    /// One item as `vnns_menu` describes it.
    private struct Item: Decodable {
        var id: String
        var title: String
        var checked: Bool
        var radio: Bool
        var enabled: Bool
        var items: [Item]

        var menuItem: GameMenuItem {
            GameMenuItem(id: id, title: title, kind: items.isEmpty ? .command : .submenu(items.map(\.menuItem)),
                         isChecked: checked, isRadio: radio, isEnabled: enabled)
        }
    }

    static func items(json: String) -> [GameMenuItem] {
        guard let items = try? JSONDecoder().decode([Item].self, from: Data(json.utf8)) else { return [] }
        return items.map(\.menuItem)
    }
}
