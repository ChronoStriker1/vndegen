import SwiftUI

nonisolated public struct FoldSplit: Equatable {
    public enum Axis: Equatable {
        case vertical
        case horizontal
    }

    public let axis: Axis
    public let size: CGSize
    public let leadingOrTop: CGRect
    public let trailingOrBottom: CGRect

    public init?(fold: CGRect?, in size: CGSize) {
        guard let fold else { return nil }
        let clipped = fold.intersection(CGRect(origin: .zero, size: size))
        guard !clipped.isNull, clipped.width > 0, clipped.height > 0 else { return nil }
        self.size = size
        if fold.height >= fold.width {
            axis = .vertical
            leadingOrTop = CGRect(x: 0, y: 0, width: clipped.minX, height: size.height)
            trailingOrBottom = CGRect(x: clipped.maxX, y: 0, width: size.width - clipped.maxX, height: size.height)
        } else {
            axis = .horizontal
            leadingOrTop = CGRect(x: 0, y: 0, width: size.width, height: clipped.minY)
            trailingOrBottom = CGRect(x: 0, y: clipped.maxY, width: size.width, height: size.height - clipped.maxY)
        }
    }

    public func insets(vertical: HorizontalEdge?, horizontal: VerticalEdge?) -> EdgeInsets {
        switch axis {
        case .vertical:
            switch vertical {
            case .leading?: EdgeInsets(top: 0, leading: 0, bottom: 0, trailing: size.width - leadingOrTop.maxX)
            case .trailing?: EdgeInsets(top: 0, leading: trailingOrBottom.minX, bottom: 0, trailing: 0)
            case nil: EdgeInsets()
            }
        case .horizontal:
            switch horizontal {
            case .top?: EdgeInsets(top: 0, leading: 0, bottom: size.height - leadingOrTop.maxY, trailing: 0)
            case .bottom?: EdgeInsets(top: trailingOrBottom.minY, leading: 0, bottom: 0, trailing: 0)
            case nil: EdgeInsets()
            }
        }
    }
}

extension GeometryProxy {
    nonisolated public func fold(includingInactive: Bool = false) -> CGRect? {
        if #available(iOS 27.1, macOS 27.1, *) {
            let options: ReservedRegion.QueryOptions = includingInactive ? .includeInactive : []
            return reservedRegions(kind: .division, options: options).first { includingInactive || $0.isActive }?.frame
        }
        return nil
    }

    nonisolated public func occlusions() -> [CGRect] {
        if #available(iOS 27.1, macOS 27.1, *) {
            return reservedRegions(kind: .occlusion).filter(\.isActive).map(\.frame)
        }
        return []
    }
}

extension View {
    public func outOfFold(vertical: HorizontalEdge? = nil, horizontal: VerticalEdge? = nil) -> some View {
        modifier(OutOfFold(vertical: vertical, horizontal: horizontal))
    }
}

private struct OutOfFold: ViewModifier {
    let vertical: HorizontalEdge?
    let horizontal: VerticalEdge?
    @State private var insets = EdgeInsets()

    func body(content: Content) -> some View {
        let vertical = vertical, horizontal = horizontal
        return content
            .padding(insets)
            .animation(.smooth(duration: 0.3), value: insets)
            .onGeometryChange(for: EdgeInsets.self) { proxy in
                FoldSplit(fold: proxy.fold(), in: proxy.size)?.insets(vertical: vertical, horizontal: horizontal) ?? EdgeInsets()
            } action: { insets = $0 }
    }
}
