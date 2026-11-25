import GameInput
import SwiftUI
import VNCore

struct MenuButton: View {
    let corner: MenuButtonCorner
    let metrics: HubMetrics
    let action: () -> Void
    let move: (MenuButtonCorner) -> Void
    @State private var drag: CGSize?

    private static let space = "menuButtonArea"
    private static let threshold: CGFloat = 10

    var body: some View {
        GeometryReader { proxy in
            Button(action: action) {
                // The same house as the touch controls' Home button, which opens the same menu.
                Image(systemName: "house.fill")
                    .font(.system(size: 16, weight: .bold))
                    .frame(width: 42, height: 42)
                    .background(.ultraThinMaterial, in: .circle)
                    .overlay { Circle().strokeBorder(.white.opacity(0.25)) }
                    .opacity(0.8)
                    .frame(width: 48, height: 48)
                    .contentShape(.circle)
            }
            .buttonStyle(.plain)
            .highPriorityGesture(dragGesture(in: proxy.size))
            .accessibilityLabel("VNDegen menu")
            .accessibilityValue(corner.title)
            .accessibilityAction(named: "Move to next corner") { move(corner.next) }
            .padding(10)
            .padding(corner.edge == .leading ? .leading : .trailing, corner.isTop ? metrics.topClearance(corner.edge) : 0)
            .offset(metrics.menuButtonClearance(corner, in: proxy.frame(in: .global)))
            .offset(drag ?? .zero)
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: corner.alignment)
        }
        .coordinateSpace(.named(Self.space))
    }

    private func dragGesture(in size: CGSize) -> some Gesture {
        DragGesture(minimumDistance: 0, coordinateSpace: .named(Self.space))
            .onChanged { value in
                if drag != nil || hypot(value.translation.width, value.translation.height) >= Self.threshold {
                    drag = value.translation
                }
            }
            .onEnded { value in
                guard drag != nil else { return action() }
                let target = MenuButtonCorner.nearest(to: value.predictedEndLocation, in: size)
                withAnimation(.spring(duration: 0.3)) {
                    drag = nil
                    move(target)
                }
            }
    }
}

extension HubMetrics {
    func menuButtonClearance(_ corner: MenuButtonCorner, in area: CGRect) -> CGSize {
        let inset = 10 + (corner.isTop ? topClearance(corner.edge) : 0)
        let button = CGRect(x: corner.edge == .leading ? inset : area.width - inset - 48,
                            y: corner.isTop ? 10 : area.height - 58, width: 48, height: 48)
        return offsetClearing([button], of: occlusions.map { $0.offsetBy(dx: -area.minX, dy: -area.minY) },
                              within: CGRect(origin: .zero, size: area.size))
    }
}

extension MenuButtonCorner {
    static let standard = MenuButtonCorner.topTrailing

    static func nearest(to point: CGPoint, in size: CGSize) -> MenuButtonCorner {
        switch (point.y < size.height / 2, point.x < size.width / 2) {
        case (true, true): .topLeading
        case (true, false): .topTrailing
        case (false, true): .bottomLeading
        case (false, false): .bottomTrailing
        }
    }

    var next: MenuButtonCorner {
        switch self {
        case .topTrailing: .bottomTrailing
        case .bottomTrailing: .bottomLeading
        case .bottomLeading: .topLeading
        case .topLeading: .topTrailing
        }
    }

    var isTop: Bool { self == .topLeading || self == .topTrailing }

    var edge: HorizontalEdge { self == .topLeading || self == .bottomLeading ? .leading : .trailing }

    var alignment: Alignment {
        switch self {
        case .topLeading: .topLeading
        case .topTrailing: .topTrailing
        case .bottomLeading: .bottomLeading
        case .bottomTrailing: .bottomTrailing
        }
    }

    var title: String {
        switch self {
        case .topLeading: "Top left"
        case .topTrailing: "Top right"
        case .bottomLeading: "Bottom left"
        case .bottomTrailing: "Bottom right"
        }
    }
}
