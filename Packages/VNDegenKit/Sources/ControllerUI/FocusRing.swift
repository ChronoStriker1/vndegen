import SwiftUI
import VNCore

/// Which control has gamepad focus, among rows of controls that the owning model derives from its state.
nonisolated public struct FocusRing<Target: Hashable> {
    public var target: Target?
    /// The ring shows only once a controller moved it, so touch users never see it.
    public var showsFocus = false

    public init() {}

    /// The focused control, or the default when the stored one is no longer on screen. A default that is not on screen
    /// either, such as a row the search filter hid, falls back to the first control.
    public func current(in rows: [[Target]], default fallback: Target? = nil) -> Target? {
        for candidate in [target, fallback] {
            if let candidate, rows.contains(where: { $0.contains(candidate) }) { return candidate }
        }
        return rows.first?.first
    }

    public func isFocused(_ candidate: Target, in rows: [[Target]], default fallback: Target? = nil) -> Bool {
        showsFocus && current(in: rows, default: fallback) == candidate
    }

    /// Up and down change rows, left and right move within one. The first press only reveals the ring where focus
    /// already is. Returns false for buttons that are not directions.
    public mutating func move(_ button: VirtualButton, in rows: [[Target]], default fallback: Target? = nil) -> Bool {
        guard [.up, .down, .left, .right].contains(button) else { return false }
        defer { showsFocus = true }
        guard let current = current(in: rows, default: fallback),
              let r = rows.firstIndex(where: { $0.contains(current) }), let c = rows[r].firstIndex(of: current) else { return true }
        guard showsFocus else { return true }
        switch button {
        case .up where r > 0: target = rows[r - 1][min(c, rows[r - 1].count - 1)]
        case .down where r < rows.count - 1: target = rows[r + 1][min(c, rows[r + 1].count - 1)]
        case .left: target = rows[r][max(0, c - 1)]
        case .right: target = rows[r][min(rows[r].count - 1, c + 1)]
        default: target = current
        }
        return true
    }

    /// Touch and pointer: focus the control without showing the ring.
    public mutating func select(_ candidate: Target) {
        target = candidate
        showsFocus = false
    }
}

/// The row background for gamepad focus: a tinted fill with a white outline, like the hub's focus ring.
public struct FocusRowBackground: View {
    public init() {}

    public var body: some View {
        RoundedRectangle(cornerRadius: 10, style: .continuous)
            .fill(.tint.opacity(0.32))
            .strokeBorder(.white, lineWidth: 2.5)
    }
}

extension View {
    /// Draws the focus ring behind a form row while it has gamepad focus, and makes the row a scroll target.
    public func focusRow(_ focused: Bool, id: some Hashable) -> some View {
        #if os(macOS)
        // A grouped form on macOS ignores `listRowBackground`, so the ring goes behind the row's content, out to the
        // row's edges.
        background { if focused { FocusRowBackground().padding(.horizontal, -10).padding(.vertical, -5) } }
            .animation(.spring(response: 0.28, dampingFraction: 0.78), value: focused)
            .id(id)
        #else
        listRowBackground(focused ? FocusRowBackground() : nil)
            .animation(.spring(response: 0.28, dampingFraction: 0.78), value: focused)
            .id(id)
        #endif
    }
}
