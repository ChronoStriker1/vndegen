import SwiftUI

extension EnvironmentValues {
    @Entry public var sheetSideBar: SideBarPlacement?
}

extension View {
    public func sheetSideBar() -> some View {
        modifier(SheetSideBarReader())
    }

    @ViewBuilder
    public func sheetSideBarClose(_ title: String, systemImage: String, shortcut: KeyboardShortcut? = nil,
                                  placement: SideBarPlacement?, action: @escaping () -> Void) -> some View {
        if let placement {
            overlay {
                SideBarColumn(placement: placement, groups: [[
                    SideBarItem(id: "close", title: title, systemImage: systemImage, shortcut: shortcut, action: action),
                ]])
            }
        } else {
            self
        }
    }
}

private struct SheetSideBarReader: ViewModifier {
    @State private var placement: SideBarPlacement?

    func body(content: Content) -> some View {
        content
            .safeAreaPadding(placement?.edge == .leading ? .leading : .trailing, placement?.widening ?? 0)
            .environment(\.sheetSideBar, placement)
            #if os(iOS)
            .background {
                SideBarReader { placement = $0 }
                    .ignoresSafeArea()
                    .accessibilityHidden(true)
            }
            #endif
    }
}
