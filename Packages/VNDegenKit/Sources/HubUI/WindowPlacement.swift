import CoreGraphics

/// Keeps a Mac window where the player can see and reach it. macOS restores the window's saved frame as it was, so a
/// frame saved on a larger display, or left there by another run of the app, can come back mostly off screen.
nonisolated public enum WindowPlacement {
    /// The least of a window's area that must stay on a screen before it is moved back.
    public static let minimumVisibleShare = 0.25

    /// `frame` moved onto a screen and capped to its visible frame, or nil when it is fine as it is. The screen is the one
    /// the frame overlaps most, or the first, the main screen, when it overlaps none. All in screen coordinates.
    public static func corrected(_ frame: CGRect, visibleFrames: [CGRect]) -> CGRect? {
        guard frame.width > 0, frame.height > 0, let main = visibleFrames.first else { return nil }
        let nearest = visibleFrames.max { overlap(frame, $0) < overlap(frame, $1) } ?? main
        let screen = overlap(frame, nearest) > 0 ? nearest : main
        let oversized = frame.width > screen.width || frame.height > screen.height
        let shown = visibleFrames.map { overlap(frame, $0) }.reduce(0, +) / (frame.width * frame.height)
        guard oversized || shown < minimumVisibleShare else { return nil }
        let width = min(frame.width, screen.width), height = min(frame.height, screen.height)
        // Kept where it was as far as the screen allows, so a window only partly off moves no further than it must.
        let x = min(max(frame.minX, screen.minX), screen.maxX - width)
        let y = min(max(frame.minY, screen.minY), screen.maxY - height)
        return CGRect(x: x, y: y, width: width, height: height)
    }

    private static func overlap(_ a: CGRect, _ b: CGRect) -> CGFloat {
        let shared = a.intersection(b)
        return shared.isNull ? 0 : shared.width * shared.height
    }
}
