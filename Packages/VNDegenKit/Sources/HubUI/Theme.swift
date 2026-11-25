import SwiftUI
import ControllerUI

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
    var foldable = false
    var foldGap: CGFloat = 0
    var sideBar: SideBarPlacement?
    var occlusions: [CGRect] = []

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
        // The full size, which the on-screen keyboard leaves alone, so opening it never reads as turning the device.
        return fullHeight > fullWidth && side > insets.top ? side : 0
    }

    /// The iPhone Duo's outer display, turned on its side, keeps its camera in a safe-area inset down one side of the
    /// screen and none down the other. iPhones whose camera sits in the Dynamic Island inset both sides alike, so this
    /// is nil for them, as it is for a portrait screen.
    var landscapeSideCamera: (edge: HorizontalEdge, width: CGFloat)? {
        guard fullWidth > fullHeight, (insets.leading == 0) != (insets.trailing == 0) else { return nil }
        return insets.leading > 0 ? (.leading, insets.leading) : (.trailing, insets.trailing)
    }

    func controlsAcrossSideCamera(_ picture: SideCameraPicture) -> Bool {
        picture == .fullWidth && landscapeSideCamera != nil
    }

    func gameClearance(_ picture: SideCameraPicture, pictureSize: CGSize?) -> EdgeInsets {
        guard picture == .clearOfCamera, let camera = landscapeSideCamera else { return EdgeInsets() }
        let beside = camera.edge == .leading
            ? EdgeInsets(top: 0, leading: camera.width, bottom: 0, trailing: 0)
            : EdgeInsets(top: 0, leading: 0, bottom: 0, trailing: camera.width)
        guard let pictureSize, pictureSize.width > 0, pictureSize.height > 0, !occlusions.isEmpty else { return beside }
        let top = max(0, occlusions.filter { $0.midY < fullHeight / 2 }.map(\.maxY).max() ?? 0)
        let bottom = max(0, occlusions.filter { $0.midY >= fullHeight / 2 }.map { fullHeight - $0.minY }.max() ?? 0)
        let besideScale = min((fullWidth - camera.width) / pictureSize.width, fullHeight / pictureSize.height)
        let bandScale = min(fullWidth / pictureSize.width, (fullHeight - top - bottom) / pictureSize.height)
        return bandScale > besideScale ? EdgeInsets(top: top, leading: 0, bottom: bottom, trailing: 0) : beside
    }

    /// These metrics for content that reaches across a side camera's inset below the camera: the full width, with the
    /// camera's corner kept clear as an iPad window's controls are, through `topRowPadding` and `cornerClearance`.
    var besideSideCamera: HubMetrics {
        let side = sideCamera
        guard side > 0, sideBar == nil else { return self }
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

    func withSideBar(_ bar: SideBarPlacement?) -> HubMetrics {
        guard let bar else { return self }
        var metrics = self
        metrics.sideBar = bar
        if bar.edge == .leading {
            metrics.size.width -= max(0, bar.width - insets.leading)
            metrics.insets.leading = max(insets.leading, bar.width)
        } else {
            metrics.size.width -= max(0, bar.width - insets.trailing)
            metrics.insets.trailing = max(insets.trailing, bar.width)
        }
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
    var headerColumns: Int { columns(maxWidth: 480, minimum: 1) }
    var headerWidth: CGFloat { cellWidth(columns: headerColumns) }

    var gridColumns: Int { columns(maxWidth: tileWidth * 1.2, minimum: 2) }
    /// Stretched so each row runs edge to edge rather than leaving a ragged gap on the right.
    var gridTileWidth: CGFloat { cellWidth(columns: gridColumns) }

    func columnSpacing(after column: Int, of columns: Int) -> CGFloat {
        columns.isMultiple(of: 2) && column == columns / 2 - 1 ? max(tileSpacing, foldGap) : tileSpacing
    }

    func gridItems(columns: Int, width: CGFloat) -> [GridItem] {
        (0..<columns).map { GridItem(.fixed(width), spacing: columnSpacing(after: $0, of: columns)) }
    }

    private func columns(maxWidth: CGFloat, minimum: Int) -> Int {
        let fitting = max(minimum, Int(((size.width - padding * 2 + tileSpacing) / (maxWidth + tileSpacing)).rounded(.up)))
        return foldable && fitting > 1 && !fitting.isMultiple(of: 2) ? fitting + 1 : fitting
    }

    private func cellWidth(columns: Int) -> CGFloat {
        let gaps = (0..<columns - 1).reduce(CGFloat(0)) { $0 + columnSpacing(after: $1, of: columns) }
        return (size.width - padding * 2 - gaps) / CGFloat(columns)
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
    var bannerHeight: CGFloat { min(540, max(180, size.width / (3840.0 / 1240.0))) }
    var titleSize: CGFloat { isCompact ? 30 : (isShort ? 30 : 44) }
}

extension View {
    /// Lays the content out across a side camera's inset, with metrics to match. See `HubMetrics.sideCamera`.
    func besideSideCamera(_ metrics: HubMetrics) -> some View {
        environment(\.hubMetrics, metrics.besideSideCamera)
            .ignoresSafeArea(.container, edges: metrics.besideSideCamera.acrossSideCamera ? .horizontal : [])
    }

    /// A form across a side camera's inset. A form takes its side margins from the system's safe area, which the hub
    /// ignores there, so without margins of its own it would run to the trailing edge.
    @ViewBuilder
    func formBesideSideCamera(_ metrics: HubMetrics) -> some View {
        if metrics.acrossSideCamera { contentMargins(.horizontal, 20, for: .scrollContent) } else { self }
    }

    /// Insets both sides by the larger horizontal safe area, so centered layouts stay centered on the screen when only
    /// one side has an inset, such as the iPhone Duo's 84 pt camera inset.
    func balancedHorizontalSafeArea(_ metrics: HubMetrics) -> some View {
        let side = max(metrics.insets.leading, metrics.insets.trailing)
        return padding(.leading, metrics.sideBar == nil ? side : metrics.insets.leading)
            .padding(.trailing, metrics.sideBar == nil ? side : metrics.insets.trailing)
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
