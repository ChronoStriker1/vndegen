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

    /// Width including the safe area, for art that runs edge to edge.
    var fullWidth: CGFloat { size.width + insets.leading + insets.trailing }
    var fullHeight: CGFloat { size.height + insets.top + insets.bottom }
    /// Horizontal content padding for views that ignore the horizontal safe area.
    var leading: CGFloat { padding + insets.leading }
    var trailing: CGFloat { padding + insets.trailing }

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

    /// Height of the home screen's hero area above the first shelf.
    var stageHeight: CGFloat {
        if isShort { return max(220, size.height * 0.66) }
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
        return min(size.height * (isShort ? 0.62 : 0.5), max(isCompact ? size.height * 0.4 : 280, wide))
    }
    var titleSize: CGFloat { isCompact ? 30 : (isShort ? 30 : 44) }
}

extension View {
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
