import CoreGraphics

nonisolated struct GamepadLayout: Equatable {
    let unit: CGFloat
    var l2, r2, l, home, view, menu, r, dpad, face: CGRect

    static func unit(for side: CGFloat) -> CGFloat { min(max(side / 6.2, 54), 76) }

    static func height(unit: CGFloat) -> CGFloat { unit * (rows + gap + bottomPadding) }

    private static let side: CGFloat = 0.3
    private static let gap: CGFloat = 0.28
    private static let bottomPadding: CGFloat = 0.25
    private static let shoulder = CGSize(width: 1.2, height: 0.6)
    private static let key: CGFloat = 0.62
    private static let keySpacing: CGFloat = 0.2
    private static let pad: CGFloat = 2.5
    private static let spread: CGFloat = 0.82
    private static let cluster: CGFloat = 1 + spread * 2
    private static let rows = shoulder.height + key + cluster + gap * 2

    init(size: CGSize, top: CGFloat = 0) {
        let u = Self.unit(for: min(size.width, size.height))
        unit = u
        let side = u * Self.side, gap = u * Self.gap, key = u * Self.key, spacing = u * Self.keySpacing
        let shoulder = CGSize(width: u * Self.shoulder.width, height: u * Self.shoulder.height)
        let cluster = u * Self.cluster, pad = u * Self.pad
        let trailing = size.width - side
        let triggers = max(top + gap, size.height - u * Self.bottomPadding - u * Self.rows)
        let bumpers = triggers + shoulder.height + gap
        let bumperShoulders = bumpers + (key - shoulder.height) / 2
        let sticks = bumpers + key + gap
        l2 = CGRect(origin: CGPoint(x: side, y: triggers), size: shoulder)
        r2 = CGRect(origin: CGPoint(x: trailing - shoulder.width, y: triggers), size: shoulder)
        l = CGRect(origin: CGPoint(x: side, y: bumperShoulders), size: shoulder)
        home = CGRect(x: side + shoulder.width + spacing, y: bumpers, width: key, height: key)
        r = CGRect(origin: CGPoint(x: trailing - shoulder.width, y: bumperShoulders), size: shoulder)
        menu = CGRect(x: r.minX - spacing - key, y: bumpers, width: key, height: key)
        view = CGRect(x: menu.minX - spacing - key, y: bumpers, width: key, height: key)
        dpad = CGRect(x: side, y: sticks + cluster - pad, width: pad, height: pad)
        face = CGRect(x: trailing - cluster, y: sticks, width: cluster, height: cluster)
    }

    var faceButtons: [CGRect] {
        let spread = unit * Self.spread
        return [CGPoint(x: spread, y: 0), CGPoint(x: 0, y: spread), CGPoint(x: spread * 2, y: spread), CGPoint(x: spread, y: spread * 2)]
            .map { CGRect(x: face.minX + $0.x, y: face.minY + $0.y, width: unit, height: unit) }
    }

    func keepingClear(of regions: [CGRect], in size: CGSize) -> GamepadLayout {
        guard !regions.isEmpty else { return self }
        let bounds = CGRect(origin: .zero, size: size)
        let controls: [WritableKeyPath<GamepadLayout, CGRect>] = [\.l2, \.r2, \.l, \.home, \.view, \.menu, \.r, \.dpad, \.face]
        var layout = self
        for path in controls {
            let others = controls.filter { $0 != path }.map { layout[keyPath: $0] }
            let targets = path == \.face ? layout.faceButtons : [layout[keyPath: path]]
            let shift = offsetClearing(targets, of: regions, within: bounds, besides: others)
            layout[keyPath: path] = layout[keyPath: path].offsetBy(dx: shift.width, dy: shift.height)
        }
        return layout
    }
}

nonisolated public func offsetClearing(_ targets: [CGRect], of regions: [CGRect], within bounds: CGRect,
                                       besides others: [CGRect] = []) -> CGSize {
    func overlaps(_ shift: CGSize, _ areas: [CGRect]) -> Bool {
        targets.contains { target in
            let moved = target.offsetBy(dx: shift.width, dy: shift.height)
            return areas.contains { area in
                let overlap = moved.intersection(area)
                return !overlap.isNull && overlap.width > 0.01 && overlap.height > 0.01
            }
        }
    }
    guard overlaps(.zero, regions) else { return .zero }
    let moves = targets.flatMap { target in
        regions.flatMap { region in
            [CGSize(width: region.maxX - target.minX, height: 0), CGSize(width: region.minX - target.maxX, height: 0),
             CGSize(width: 0, height: region.maxY - target.minY), CGSize(width: 0, height: region.minY - target.maxY)]
        }
    }
    let inBounds = bounds.insetBy(dx: -0.01, dy: -0.01)
    return moves
        .filter { shift in
            targets.allSatisfy { inBounds.contains($0.offsetBy(dx: shift.width, dy: shift.height)) }
                && !overlaps(shift, regions + others)
        }
        .min { abs($0.width) + abs($0.height) < abs($1.width) + abs($1.height) } ?? .zero
}
