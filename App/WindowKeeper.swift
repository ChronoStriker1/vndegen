#if os(macOS)
import AppKit
import HubUI
import SwiftUI

/// Brings the app's window onto a screen when it opens. macOS restores the window's saved frame as it was, so a frame
/// saved on another display arrangement, or saved while a window manager had parked the window off screen, can open it
/// mostly out of sight. It acts once, on the frame the window opens with: a window manager that moves the window later,
/// such as a tiling one hiding it with a workspace, does so on purpose, and checking every move would fight it.
struct WindowKeeper: NSViewRepresentable {
    func makeNSView(context: Context) -> KeeperView { KeeperView() }
    func updateNSView(_ view: KeeperView, context: Context) {}

    final class KeeperView: NSView {
        private var placed = false

        override func viewDidMoveToWindow() {
            super.viewDidMoveToWindow()
            guard !placed, let window, !window.styleMask.contains(.fullScreen) else { return }
            placed = true
            if let frame = WindowPlacement.corrected(window.frame, visibleFrames: NSScreen.screens.map(\.visibleFrame)) {
                #if DEBUG
                NSLog("WINDOWKEEPER moved %@ to %@", NSStringFromRect(window.frame), NSStringFromRect(frame))
                #endif
                window.setFrame(frame, display: true)
            }
        }

        override func hitTest(_ point: NSPoint) -> NSView? { nil }
    }
}
#endif
