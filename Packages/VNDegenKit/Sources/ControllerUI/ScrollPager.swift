import SwiftUI

/// Scrolls a scroll view that has nothing to focus, such as a license's text, by controller presses. Attach it with
/// `scrollPager(_:)`, then call `scroll(pages:)`.
@Observable
public final class ScrollPager {
    var position = ScrollPosition(edge: .top)
    @ObservationIgnored private var geometry = Geometry()

    struct Geometry: Equatable {
        var offset: CGFloat = 0
        var visible: CGFloat = 0
        var content: CGFloat = 0
        var top: CGFloat = 0
        var bottom: CGFloat = 0
    }

    public init() {}

    /// Scrolls by `pages` heights of the visible area, back for a negative number, less a line's worth of a whole page
    /// so the reader keeps their place.
    public func scroll(pages: Double) {
        let target = geometry.offset + (geometry.visible - 40) * pages
        let limit = max(-geometry.top, geometry.content + geometry.bottom - geometry.visible)
        withAnimation(.easeOut(duration: 0.25)) { position.scrollTo(y: min(max(-geometry.top, target), limit)) }
    }

    func update(_ geometry: Geometry) {
        self.geometry = geometry
    }
}

extension View {
    /// Lets `pager` scroll this scroll view.
    public func scrollPager(_ pager: ScrollPager) -> some View {
        scrollPosition(Binding(get: { pager.position }, set: { pager.position = $0 }))
            .onScrollGeometryChange(for: ScrollPager.Geometry.self) { geo in
                ScrollPager.Geometry(offset: geo.contentOffset.y, visible: geo.containerSize.height, content: geo.contentSize.height,
                                     top: geo.contentInsets.top, bottom: geo.contentInsets.bottom)
            } action: { _, geometry in
                pager.update(geometry)
            }
    }
}
