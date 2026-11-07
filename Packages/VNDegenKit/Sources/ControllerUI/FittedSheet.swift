import SwiftUI

extension View {
    /// Sizes a sheet to its content: this view's height, grown or shrunk by how much the scroll view marked with
    /// `reportsSheetOverflow()` inside it over- or under-fills its room. The system keeps the height within the screen,
    /// so a long list still opens as tall as the screen allows. `fallback` applies until the first measurement.
    public func fittedSheetHeight(fallback: Set<PresentationDetent> = [.medium, .large]) -> some View {
        modifier(FittedSheetHeight(fallback: fallback))
    }

    /// Marks the scroll view or form whose content `fittedSheetHeight` fits the sheet to.
    public func reportsSheetOverflow() -> some View {
        modifier(ReportsSheetOverflow())
    }
}

/// The marked scroll view's content height and the height it is given.
private struct SheetScrollKey: PreferenceKey {
    static let defaultValue: SheetScroll? = nil
    static func reduce(value: inout SheetScroll?, nextValue: () -> SheetScroll?) { value = nextValue() ?? value }
}

private struct SheetScroll: Equatable {
    var content: CGFloat
    var container: CGFloat
}

private struct ReportsSheetOverflow: ViewModifier {
    @State private var scroll: SheetScroll?

    func body(content: Content) -> some View {
        content
            .onScrollGeometryChange(for: SheetScroll.self) {
                SheetScroll(content: $0.contentSize.height, container: $0.containerSize.height)
            } action: { _, new in
                scroll = new
            }
            .preference(key: SheetScrollKey.self, value: scroll)
    }
}

/// The sheet's height is its chrome, everything but the scroll view, plus the scroll view's content. Both change
/// together as the sheet resizes, so a new height never asks for another. Insets are left out on purpose: the system
/// adds them only at some heights, which would make the height chase itself.
private struct FittedSheetHeight: ViewModifier {
    let fallback: Set<PresentationDetent>
    @Environment(\.cameraCornerTrailing) private var cameraCorner
    @State private var height: CGFloat = 0
    @State private var scroll: SheetScroll?
    /// The height the sheet was given. It moves only for a real change in the content, never for the frame or two in
    /// which the sheet and its scroll view resize one after the other.
    @State private var applied: CGFloat?

    func body(content: Content) -> some View {
        content
            .onGeometryChange(for: CGFloat.self) { $0.size.height } action: { height = $0 }
            .onPreferenceChange(SheetScrollKey.self) { scroll = $0 }
            .onChange(of: fitted) { _, fitted in
                guard let fitted, abs(fitted - (applied ?? 0)) > 8 else { return }
                applied = fitted
            }
            // Beside a camera in the top corner, every sheet opens full height. See `cameraCornerTrailing`.
            .presentationDetents(cameraCorner > 0 ? [.large] : applied.map { [.height($0)] } ?? fallback)
    }

    private var fitted: CGFloat? {
        guard height > 0, let scroll, scroll.container > 0 else { return nil }
        return (height - scroll.container + scroll.content).rounded()
    }
}
