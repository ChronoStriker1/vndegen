import SwiftUI
import VNCore

/// Shows where the virtual mouse's pointer is, once a button or stick moved it. On iPhone and iPad it draws an arrow.
/// On a Mac it moves the system pointer there, so the real mouse carries on from where the controller left it.
/// Place it over the session view at the same size, so its normalized locations match the game's. It takes no clicks
/// or touches.
public struct PointerView: View {
    let mouse: VirtualMouse
    /// False while the touch cursor draws the same pointer itself.
    let drawsArrow: Bool

    public init(mouse: VirtualMouse, drawsArrow: Bool = true) {
        self.mouse = mouse
        self.drawsArrow = drawsArrow
    }

    public var body: some View {
        GeometryReader { geometry in
            #if os(macOS)
            SystemPointer(mouse: mouse, location: mouse.isShown ? mouse.location : nil)
            #else
            if drawsArrow, mouse.isShown { PointerArrow(location: mouse.location, size: geometry.size) }
            #endif
        }
        .onGeometryChange(for: CGSize.self) { $0.size } action: { mouse.viewSize = $0 }
        .allowsHitTesting(false)
        .accessibilityHidden(true)
    }
}

/// The cursor mode's and the controller's pointer.
struct PointerArrow: View {
    let location: CGPoint
    let size: CGSize

    var body: some View {
        Image(systemName: "cursorarrow")
            .font(.system(size: 26, weight: .regular))
            .foregroundStyle(.white)
            .shadow(color: .black.opacity(0.7), radius: 2)
            // The arrow's tip sits about 7 by 3 points into the symbol's frame.
            .offset(x: location.x * size.width - 7, y: location.y * size.height - 3)
            .allowsHitTesting(false)
    }
}

#if os(macOS)
import AppKit

/// Moves the system pointer to where the virtual mouse is, and tells it where the system pointer is.
private struct SystemPointer: NSViewRepresentable {
    let mouse: VirtualMouse
    /// Nil until a button or stick moved the pointer.
    let location: CGPoint?

    func makeNSView(context: Context) -> PointerNSView {
        let view = PointerNSView()
        // Showing again, such as when the VNDegen menu closes, leaves the pointer where the player put it.
        view.warped = location
        mouse.systemPointer = { [weak view] in view?.systemLocation }
        return view
    }

    func updateNSView(_ view: PointerNSView, context: Context) {
        if let location { view.warp(to: location) }
    }

    final class PointerNSView: NSView {
        /// The last place the pointer was moved to, so a layout pass alone never moves it.
        var warped: CGPoint?

        override var isFlipped: Bool { true }
        override func hitTest(_ point: NSPoint) -> NSView? { nil }

        /// The system pointer over this view, normalized. Nil when it is outside the view or the app is in the background.
        var systemLocation: CGPoint? {
            guard let window, NSApp.isActive, bounds.width > 0, bounds.height > 0 else { return nil }
            let point = convert(window.convertPoint(fromScreen: NSEvent.mouseLocation), from: nil)
            guard bounds.contains(point) else { return nil }
            return CGPoint(x: point.x / bounds.width, y: point.y / bounds.height)
        }

        func warp(to location: CGPoint) {
            guard location != warped, let window, NSApp.isActive, let screen = NSScreen.screens.first else { return }
            warped = location
            let point = window.convertPoint(toScreen: convert(CGPoint(x: location.x * bounds.width,
                                                                      y: location.y * bounds.height), to: nil))
            // Quartz counts from the top of the main display, AppKit from its bottom.
            CGWarpMouseCursorPosition(CGPoint(x: point.x, y: screen.frame.maxY - point.y))
            // Without this the real mouse stalls for a quarter second after each move.
            CGAssociateMouseAndMouseCursorPosition(1)
        }
    }
}
#endif
