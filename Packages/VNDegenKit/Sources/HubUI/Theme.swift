import SwiftUI

enum Theme {
    static let background = Color(red: 0.035, green: 0.035, blue: 0.06)
    static let surface = Color.white.opacity(0.07)
    static let stroke = Color.white.opacity(0.12)
    static let accent = Color(red: 1.0, green: 0.25, blue: 0.5)
    static let secondaryText = Color.white.opacity(0.64)
    static let tertiaryText = Color.white.opacity(0.42)
    static let focusAnimation = Animation.spring(response: 0.28, dampingFraction: 0.78)
    static let heroAnimation = Animation.easeInOut(duration: 0.45)
}

extension Font {
    /// The hub's display face: SF expanded, for titles and section labels that should read like game UI.
    static func display(_ size: CGFloat, weight: Font.Weight = .heavy) -> Font {
        .system(size: size, weight: weight).width(.expanded)
    }
}

/// Every size the hub derives from its container, so layout follows the window or display and never the device model.
/// The iPhone Duo changes size when folded, and a Mac window resizes freely, so these are recomputed on every size change.
struct HubMetrics: Equatable {
    /// The safe area's size.
    var size: CGSize
    var insets = EdgeInsets()
    var windowControls = WindowControlsInsets()
    /// True for metrics from `besideSideCamera`, whose content reaches across the camera's inset.
    var acrossSideCamera = false

    /// Width including the safe area, for art that runs edge to edge.
    var fullWidth: CGFloat { size.width + insets.leading + insets.trailing }
    var fullHeight: CGFloat { size.height + insets.top + insets.bottom }
    /// Horizontal content padding for views that ignore the horizontal safe area.
    var leading: CGFloat { padding + insets.leading }
    var trailing: CGFloat { padding + insets.trailing }

    /// How far past the safe area's `edge` an iPad window's controls reach in the top row. Zero without controls there.
    func topClearance(_ edge: HorizontalEdge) -> CGFloat {
        max(0, edge == .leading ? windowControls.leading - insets.leading : windowControls.trailing - insets.trailing)
    }

    /// Padding from the safe area's `edge` for a view in the top row, which also keeps it beside an iPad window's controls.
    func topRowPadding(_ edge: HorizontalEdge) -> CGFloat {
        let clearance = topClearance(edge)
        return clearance > 0 ? max(padding, clearance + 8) : padding
    }

    /// How far below the safe area's top a view in a top corner starts, to sit below an iPad window's controls.
    var cornerClearance: CGFloat { max(0, windowControls.top - insets.top) }

    /// The iPhone Duo's outer display, held upright, keeps its camera in a safe-area inset down the whole of one side,
    /// though the camera itself sits at the top of it. Elsewhere a side inset comes with a taller top inset or a
    /// landscape screen, and this is zero. Here it is the inset's width, which the hub also takes as the camera's
    /// height, since UIKit reports nothing finer.
    var sideCamera: CGFloat {
        let side = max(insets.leading, insets.trailing)
        return size.height > size.width && side > insets.top ? side : 0
    }

    /// These metrics for content that reaches across a side camera's inset below the camera: the full width, with the
    /// camera's corner kept clear as an iPad window's controls are, through `topRowPadding` and `cornerClearance`.
    var besideSideCamera: HubMetrics {
        let side = sideCamera
        guard side > 0 else { return self }
        var metrics = self
        metrics.size.width += insets.leading + insets.trailing
        metrics.windowControls.leading = max(windowControls.leading, insets.leading)
        metrics.windowControls.trailing = max(windowControls.trailing, insets.trailing)
        metrics.windowControls.top = max(windowControls.top, insets.top + side)
        metrics.insets.leading = 0
        metrics.insets.trailing = 0
        metrics.acrossSideCamera = true
        return metrics
    }

    var topBarSpacing: CGFloat { isCompact ? 8 : 12 }

    /// Whether the home screen's top bar fits its `buttons` beside an iPad window's controls, with the wordmark hidden.
    /// When it doesn't, the bar moves below the controls and takes the full width.
    func topBarFitsBesideControls(buttons: Int) -> Bool {
        let content = CGFloat(buttons) * 44 + CGFloat(buttons + 1) * topBarSpacing
        return content + topRowPadding(.leading) + topRowPadding(.trailing) <= size.width
    }

    var isCompact: Bool { size.width < 600 }
    /// Landscape phones and short windows, where the hero has to share very little height with the shelves.
    var isShort: Bool { size.height < 520 }
    var padding: CGFloat { isCompact ? 20 : min(64, max(28, size.width * 0.04)) }
    var tileSpacing: CGFloat { isCompact ? 12 : 18 }

    var tileWidth: CGFloat {
        let byWidth = isCompact ? (size.width - padding * 2 - tileSpacing * 2) / 2.6 : size.width / 7.2
        let byHeight = size.height * (isShort ? 0.3 : 0.25)
        return min(220, max(92, min(byWidth, byHeight)))
    }
    var tileSize: CGSize { CGSize(width: tileWidth, height: tileWidth * 1.5) }

    /// Capsule width in the capsule rows layout. With no hero above them, the shelves have the height for larger art.
    var largeTileWidth: CGFloat {
        let byWidth = isCompact ? (size.width - padding * 2 - tileSpacing) / 2.2 : size.width / 5.5
        return min(300, max(110, min(byWidth, size.height * 0.36)))
    }

    /// Columns in the header list: one on a phone held upright, two or more as the width allows.
    var headerColumns: Int { max(1, Int((size.width - padding * 2 + tileSpacing) / (320 + tileSpacing))) }
    var headerWidth: CGFloat {
        let columns = CGFloat(headerColumns)
        return (size.width - padding * 2 - tileSpacing * (columns - 1)) / columns
    }

    /// Columns in the grid layout: three on a phone, and on wider screens as many capsules of the shelves' size as fit.
    var gridColumns: Int {
        let available = size.width - padding * 2
        let target = isCompact ? (available - tileSpacing * 2) / 3 : tileWidth
        return max(2, Int((available + tileSpacing) / (target + tileSpacing)))
    }
    /// Stretched so each row runs edge to edge rather than leaving a ragged gap on the right.
    var gridTileWidth: CGFloat {
        let columns = CGFloat(gridColumns)
        return (size.width - padding * 2 - tileSpacing * (columns - 1)) / columns
    }

    /// Columns in a game page's screenshot grid `width` wide: two on a phone held upright, and more as the width allows,
    /// so the tiles grow with the window between steps.
    func screenshotColumns(in width: CGFloat) -> Int {
        let minimum: CGFloat = isCompact ? 150 : 230
        return max(2, Int((width + tileSpacing) / (minimum + tileSpacing)))
    }

    /// Height of the home screen's hero area above the first shelf.
    var stageHeight: CGFloat {
        if isShort { return max(220, size.height * 0.66) }
        // Room for the title, facts, and buttons, so a portrait phone shows the shelves sooner.
        if isCompact { return max(300, size.height * 0.4) }
        let share = size.width > size.height ? 0.6 : 0.56
        return min(780, max(300, size.height * share))
    }
    var logoMaxSize: CGSize {
        if isShort { return CGSize(width: size.width * 0.34, height: 76) }
        if isCompact { return CGSize(width: size.width * 0.72, height: 120) }
        return CGSize(width: min(520, size.width * 0.38), height: min(190, size.height * 0.2))
    }
    var bannerHeight: CGFloat {
        let wide = size.width / (3840.0 / 1240.0)
        return min(size.height * (isShort ? 0.62 : 0.5), max(isCompact ? size.height * 0.28 : 280, wide))
    }
    var titleSize: CGFloat { isCompact ? 30 : (isShort ? 30 : 44) }
}

extension View {
    /// Lays the content out across a side camera's inset, with metrics to match. See `HubMetrics.sideCamera`.
    func besideSideCamera(_ metrics: HubMetrics) -> some View {
        environment(\.hubMetrics, metrics.besideSideCamera)
            .ignoresSafeArea(.container, edges: metrics.sideCamera > 0 ? .horizontal : [])
    }

    /// A form across a side camera's inset. A form takes its side margins from the system's safe area, which the hub
    /// ignores there, so without margins of its own it would run to the trailing edge.
    @ViewBuilder
    func formBesideSideCamera(_ metrics: HubMetrics) -> some View {
        if metrics.acrossSideCamera { contentMargins(.horizontal, 20, for: .scrollContent) } else { self }
    }

    /// A sheet's content under metrics from `besideSideCamera`: across the camera's inset too, with its header's
    /// trailing button kept clear of the camera when the sheet reaches up beside it.
    func sheetBesideSideCamera() -> some View {
        modifier(SheetBesideSideCamera())
    }

    /// Insets both sides by the larger horizontal safe area, so centered layouts stay centered on the screen when only
    /// one side has an inset, such as the iPhone Duo's 84 pt camera inset.
    func balancedHorizontalSafeArea(_ metrics: HubMetrics) -> some View {
        padding(.horizontal, max(metrics.insets.leading, metrics.insets.trailing))
            .ignoresSafeArea(edges: .horizontal)
    }
}

extension EnvironmentValues {
    @Entry var hubMetrics = HubMetrics(size: CGSize(width: 1024, height: 768))
}

/// Lowercase section label with wide tracking, used above shelves and info groups.
struct SectionLabel: View {
    let text: String
    var systemImage: String?

    var body: some View {
        HStack(spacing: 6) {
            if let systemImage { Image(systemName: systemImage) }
            Text(text.uppercased())
        }
        .font(.system(size: 12, weight: .bold).width(.expanded))
        .tracking(1.6)
        .foregroundStyle(Theme.secondaryText)
    }
}

extension TimeInterval {
    /// "12h 30m", "45m", or nil when the game was never played.
    var playTimeText: String? {
        guard self >= 60 else { return nil }
        let minutes = Int(self / 60)
        return minutes >= 60 ? "\(minutes / 60)h \(minutes % 60)m" : "\(minutes)m"
    }
}

private struct SheetBesideSideCamera: ViewModifier {
    @Environment(\.hubMetrics) private var metrics
    /// The sheet's top in the window.
    @State private var top = CGFloat.infinity

    func body(content: Content) -> some View {
        let across = metrics.acrossSideCamera
        // A sheet that reaches up beside the camera moves only its header's trailing button clear of it.
        let besideCamera = across && top < metrics.cornerClearance
        content
            .environment(\.cameraCornerTrailing, besideCamera ? metrics.windowControls.trailing : 0)
            .ignoresSafeArea(.container, edges: across ? .horizontal : [])
            .onGeometryChange(for: CGFloat.self) { $0.frame(in: .global).minY } action: { top = $0 }
    }
}
