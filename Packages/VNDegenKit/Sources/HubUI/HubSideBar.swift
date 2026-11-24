import SwiftUI
import ControllerUI

struct HubSideBar: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav
    let placement: SideBarPlacement

    var body: some View {
        SideBarColumn(placement: placement, groups: groups)
    }

    private var groups: [[SideBarItem]] {
        switch nav.route {
        case nil:
            let lock = item(.lockButton, hub.lock.isUnlocked ? "Hide hidden games" : "Show hidden games",
                            hub.lock.isUnlocked ? "lock.open.fill" : "lock.fill")
            let settings = item(.settingsButton, "Settings", "gearshape.fill")
            if nav.visibleGames.isEmpty { return [[lock], [settings]] }
            return [[item(.addGame, "Add game", "plus")],
                    [item(.layoutButton, "Library view, \(nav.layout.title)", nav.layout.systemImage), lock],
                    [settings]]
        case .game, .settings:
            return [[item(.back, "Back", "chevron.left")]]
        }
    }

    private func item(_ target: Navigator.Target, _ title: String, _ systemImage: String) -> SideBarItem {
        SideBarItem(id: "\(target)", title: title, systemImage: systemImage, focused: nav.isFocused(target),
                    action: { nav.select(target) }, hover: { if $0 { nav.hover(target) } })
    }
}
