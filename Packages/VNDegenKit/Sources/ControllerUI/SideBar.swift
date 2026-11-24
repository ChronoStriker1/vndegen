import SwiftUI
#if os(iOS)
import UIKit
#endif

nonisolated public struct SideBarPlacement: Equatable {
    public static let minimumWidth: CGFloat = 72
    public static let margin: CGFloat = 8

    public let edge: HorizontalEdge
    public let width: CGFloat
    public let top: CGFloat
    public let bottom: CGFloat

    init(edge: HorizontalEdge, width: CGFloat, top: CGFloat, bottom: CGFloat) {
        self.edge = edge
        self.width = width
        self.top = top
        self.bottom = bottom
    }

    public init(edge: HorizontalEdge, size: CGSize, insets: EdgeInsets, occlusions: [CGRect]) {
        self.edge = edge
        width = max(edge == .leading ? insets.leading : insets.trailing, Self.minimumWidth)
        let column = CGRect(x: edge == .leading ? 0 : size.width - width, y: 0, width: width, height: size.height)
        var top = insets.top
        var bottom = insets.bottom
        for region in occlusions where region.intersects(column) {
            if region.midY < size.height / 2 {
                top = max(top, region.maxY)
            } else {
                bottom = max(bottom, size.height - region.minY)
            }
        }
        self.top = top + Self.margin
        self.bottom = bottom + Self.margin
    }
}

nonisolated public enum SideBarFit {
    public static let item: CGFloat = 44
    public static let spacing: CGFloat = 4
    public static let padding: CGFloat = 4
    public static let groupSpacing: CGFloat = 12

    public static func height(_ groups: [Int]) -> CGFloat {
        let filled = groups.filter { $0 > 0 }
        guard !filled.isEmpty else { return 0 }
        let capsules = filled.reduce(CGFloat(0)) { $0 + CGFloat($1) * item + CGFloat($1 - 1) * spacing + padding * 2 }
        return capsules + CGFloat(filled.count - 1) * groupSpacing
    }

    public static func shown(_ groups: [Int], in available: CGFloat) -> Int {
        let total = groups.reduce(0, +)
        guard height(groups) > available else { return total }
        let overflow = height([1]) + groupSpacing
        for count in stride(from: total - 1, through: 0, by: -1) where height(leading(count, of: groups)) + overflow <= available {
            return count
        }
        return 0
    }

    static func leading(_ count: Int, of groups: [Int]) -> [Int] {
        var left = count
        return groups.map { size in
            let taken = min(size, left)
            left -= taken
            return taken
        }
    }
}

public struct SideBarItem: Identifiable {
    public let id: String
    public let title: String
    public let systemImage: String
    public let focused: Bool
    public let action: () -> Void
    public let hover: (Bool) -> Void

    public init(id: String, title: String, systemImage: String, focused: Bool = false,
                action: @escaping () -> Void, hover: @escaping (Bool) -> Void = { _ in }) {
        self.id = id
        self.title = title
        self.systemImage = systemImage
        self.focused = focused
        self.action = action
        self.hover = hover
    }
}

public struct SideBarColumn: View {
    let placement: SideBarPlacement
    let groups: [[SideBarItem]]

    public init(placement: SideBarPlacement, groups: [[SideBarItem]]) {
        self.placement = placement
        self.groups = groups
    }

    public var body: some View {
        GeometryReader { proxy in
            let shown = SideBarFit.shown(groups.map(\.count), in: proxy.size.height - placement.top - placement.bottom)
            let visible = zip(groups, SideBarFit.leading(shown, of: groups.map(\.count))).map { Array($0.prefix($1)) }
            let rest = Array(groups.joined().dropFirst(shown))
            VStack(spacing: SideBarFit.groupSpacing) {
                ForEach(Array(visible.enumerated()), id: \.offset) { _, items in
                    if !items.isEmpty {
                        capsule { ForEach(items) { button($0) } }
                    }
                }
                if !rest.isEmpty {
                    capsule {
                        Menu {
                            ForEach(rest) { item in
                                Button(item.title, systemImage: item.systemImage, action: item.action)
                            }
                        } label: {
                            Image(systemName: "ellipsis")
                                .font(.system(size: 17, weight: .bold))
                                .frame(width: SideBarFit.item, height: SideBarFit.item)
                                .contentShape(.circle)
                        }
                        .accessibilityLabel("More")
                    }
                }
            }
            .foregroundStyle(.white)
            .frame(width: placement.width)
            .padding(.top, placement.top)
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: placement.edge == .leading ? .topLeading : .topTrailing)
        }
        .ignoresSafeArea()
    }

    private func capsule(@ViewBuilder _ content: () -> some View) -> some View {
        VStack(spacing: SideBarFit.spacing) { content() }
            .padding(SideBarFit.padding)
            .background(.ultraThinMaterial, in: Capsule(style: .circular))
            .overlay { Capsule(style: .circular).strokeBorder(.white.opacity(0.12)) }
    }

    private func button(_ item: SideBarItem) -> some View {
        Button(action: item.action) {
            Image(systemName: item.systemImage)
                .font(.system(size: 17, weight: .bold))
                .foregroundStyle(item.focused ? .black : .white)
                .frame(width: SideBarFit.item, height: SideBarFit.item)
                .background { if item.focused { Circle().fill(.white) } }
                .contentShape(.circle)
        }
        .buttonStyle(.plain)
        .onHover(perform: item.hover)
        .accessibilityLabel(item.title)
    }
}

#if os(iOS)
public struct VerticalBarEdgeReader: UIViewRepresentable {
    let onChange: (HorizontalEdge?) -> Void

    public init(onChange: @escaping (HorizontalEdge?) -> Void) {
        self.onChange = onChange
    }

    public func makeUIView(context: Context) -> Probe {
        let probe = Probe()
        probe.isUserInteractionEnabled = false
        probe.onChange = onChange
        return probe
    }

    public func updateUIView(_ probe: Probe, context: Context) { probe.onChange = onChange }

    public final class Probe: UIView {
        var onChange: (HorizontalEdge?) -> Void = { _ in }
        private var reported: HorizontalEdge??

        override init(frame: CGRect) {
            super.init(frame: frame)
            if #available(iOS 27.1, *) {
                registerForTraitChanges(UITraitCollection.systemTraitsAffectingVerticalBarEdge) { (probe: Probe, _) in probe.report() }
            }
        }

        required init?(coder: NSCoder) { nil }

        override public func didMoveToWindow() {
            super.didMoveToWindow()
            report()
        }

        override public func layoutSubviews() {
            super.layoutSubviews()
            report()
        }

        private func report() {
            guard #available(iOS 27.1, *), let window else { return }
            let frame = convert(bounds, to: window)
            let ltr = effectiveUserInterfaceLayoutDirection == .leftToRight
            let edge: HorizontalEdge? = switch traitCollection.verticalBarEdge {
            case .leading: (ltr ? frame.minX : window.bounds.maxX - frame.maxX) < 1 ? .leading : nil
            case .trailing: (ltr ? window.bounds.maxX - frame.maxX : frame.minX) < 1 ? .trailing : nil
            default: nil
            }
            guard reported != .some(edge) else { return }
            reported = .some(edge)
            Task { @MainActor [onChange] in onChange(edge) }
        }
    }
}
#endif

extension GeometryProxy {
    nonisolated public func occlusions() -> [CGRect] {
        if #available(iOS 27.1, macOS 27.1, *) {
            return reservedRegions(kind: .occlusion).filter(\.isActive).map(\.frame)
        }
        return []
    }
}
