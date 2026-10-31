import SwiftUI
import VNCore

/// A game page's screenshots in a grid of 16:9 tiles. The column count follows the width, so a phone held upright shows
/// two and wider windows more, and long lists show two rows until the player asks for all of them.
struct ScreenshotGrid: View {
    @Environment(Navigator.self) private var nav
    @Environment(\.hubMetrics) private var metrics
    @Environment(\.displayScale) private var displayScale
    let screenshots: [Screenshot]
    @State private var width: CGFloat = 0

    var body: some View {
        let columns = metrics.screenshotColumns(in: width)
        let spacing = metrics.tileSpacing
        let tileWidth = max(0, (width - spacing * CGFloat(columns - 1)) / CGFloat(columns))
        let shown = Navigator.shownScreenshots(screenshots.count, columns: columns, showsAll: nav.showsAllScreenshots)
        VStack(alignment: .leading, spacing: 16) {
            LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: spacing), count: columns), spacing: spacing) {
                ForEach(0..<shown, id: \.self) { index in
                    ScreenshotTile(screenshot: screenshots[index], index: index, count: screenshots.count,
                                   sharp: tileWidth * displayScale > Self.thumbnailWidth)
                }
            }
            .onGeometryChange(for: CGFloat.self) { $0.size.width } action: { width = $0 }
            if Navigator.collapsesScreenshots(screenshots.count, columns: columns) {
                HubButton(target: .pageScreenshotsMore, kind: .secondary) {
                    if nav.showsAllScreenshots {
                        Label("Show fewer", systemImage: "chevron.up")
                    } else {
                        Label("Show all \(screenshots.count)", systemImage: "chevron.down")
                    }
                }
            }
        }
        .onChange(of: columns, initial: true) { _, columns in nav.screenshotColumns = columns }
    }

    /// Steam's thumbnails are 600 pixels wide and IGDB's 569, so tiles drawn wider than this load the full image.
    static let thumbnailWidth: CGFloat = 600
}

private struct ScreenshotTile: View {
    @Environment(Navigator.self) private var nav
    let screenshot: Screenshot
    let index: Int
    let count: Int
    /// Loads the full-size image instead of the thumbnail, for tiles too large for it.
    let sharp: Bool

    var body: some View {
        let target = Navigator.Target.pageScreenshot(index)
        Button { nav.select(target) } label: {
            Color.clear
                .aspectRatio(16 / 9, contentMode: .fit)
                .overlay {
                    ScreenshotImage(url: sharp ? screenshot.url : screenshot.thumbnail,
                                    placeholder: sharp ? screenshot.thumbnail : nil, contentMode: .fill)
                }
                .clipShape(.rect(cornerRadius: 10, style: .continuous))
                .contentShape(.rect(cornerRadius: 10, style: .continuous))
        }
        .buttonStyle(TileButtonStyle(focused: nav.isFocused(target)))
        .onHover { if $0 { nav.hover(target) } }
        .id(target)
        .accessibilityLabel("Screenshot \(index + 1) of \(count)")
    }
}

/// A remote screenshot through `AsyncImage`, which keeps what it loads in the shared URL cache. A placeholder image,
/// such as the thumbnail the grid already loaded, shows until the image arrives.
private struct ScreenshotImage: View {
    let url: URL
    var placeholder: URL?
    let contentMode: ContentMode

    var body: some View {
        AsyncImage(url: url, transaction: Transaction(animation: .easeOut(duration: 0.2))) { phase in
            switch phase {
            case let .success(image):
                image.resizable().aspectRatio(contentMode: contentMode)
            case .failure:
                ZStack {
                    Theme.surface
                    Image(systemName: "photo").font(.system(size: 22)).foregroundStyle(Theme.tertiaryText)
                }
            default:
                if let placeholder {
                    ScreenshotImage(url: placeholder, contentMode: contentMode)
                } else {
                    Theme.surface
                }
            }
        }
    }
}

/// The screenshot the full-screen viewer shows, and the rest of the page's screenshots to move through.
@MainActor
@Observable
final class ScreenshotViewer {
    let gameID: GameID
    let screenshots: [Screenshot]
    var index: Int

    init(gameID: GameID, screenshots: [Screenshot], index: Int) {
        self.gameID = gameID
        self.screenshots = screenshots
        self.index = min(max(0, index), screenshots.count - 1)
    }

    func step(_ offset: Int) {
        let next = min(max(0, index + offset), screenshots.count - 1)
        guard next != index else { return }
        withAnimation(.smooth(duration: 0.3)) { index = next }
    }
}

/// Screenshots full screen, one at a time. Swipes, the arrow buttons, a controller's left and right, and the arrow
/// keys move between them. B, Escape, and the close button leave.
struct ScreenshotViewerView: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav
    @Environment(\.hubMetrics) private var metrics
    let viewer: ScreenshotViewer

    var body: some View {
        let insets = metrics.insets
        ScrollView(.horizontal) {
            LazyHStack(spacing: 0) {
                ForEach(viewer.screenshots.indices, id: \.self) { index in
                    let screenshot = viewer.screenshots[index]
                    ScreenshotImage(url: screenshot.url, placeholder: screenshot.thumbnail, contentMode: .fit)
                        .padding(.top, insets.top + 8)
                        .padding(.bottom, insets.bottom + 8)
                        .padding(.leading, insets.leading)
                        .padding(.trailing, insets.trailing)
                        .containerRelativeFrame([.horizontal, .vertical])
                        .accessibilityElement()
                        .accessibilityLabel("Screenshot \(index + 1) of \(viewer.screenshots.count)")
                }
            }
            .scrollTargetLayout()
        }
        .scrollTargetBehavior(.paging)
        // Centered, since without an anchor a scroll to the next screenshot stops once it is visible, short of the page.
        .scrollPosition(id: Binding<Int?>(get: { viewer.index }, set: { if let index = $0 { viewer.index = index } }), anchor: .center)
        .scrollIndicators(.hidden)
        .background(.black)
        .ignoresSafeArea()
        .overlay(alignment: .topLeading) {
            CircleButton(systemImage: "xmark", label: "Close") { nav.back() }
                .padding(.leading, insets.leading + metrics.topRowPadding(.leading))
                .padding(.top, insets.top + metrics.cornerClearance + 10)
                #if os(macOS)
                .padding(.top, 18)
                #endif
        }
        .overlay(alignment: .bottom) {
            Text("\(viewer.index + 1) of \(viewer.screenshots.count)")
                .font(.system(size: 13, weight: .semibold).monospacedDigit())
                .padding(.horizontal, 14)
                .padding(.vertical, 7)
                .background(.ultraThinMaterial, in: .capsule)
                .padding(.bottom, max(insets.bottom, 16))
                .accessibilityHidden(true)
        }
        .overlay {
            // Touch has swipes. The pointer, keyboard, and controller get arrows, which also show where left and right go.
            if hub.input.lastInputKind != .touch, viewer.screenshots.count > 1 {
                HStack {
                    CircleButton(systemImage: "chevron.left", label: "Previous screenshot") { viewer.step(-1) }
                        .opacity(viewer.index > 0 ? 1 : 0)
                        .allowsHitTesting(viewer.index > 0)
                    Spacer()
                    CircleButton(systemImage: "chevron.right", label: "Next screenshot") { viewer.step(1) }
                        .opacity(viewer.index < viewer.screenshots.count - 1 ? 1 : 0)
                        .allowsHitTesting(viewer.index < viewer.screenshots.count - 1)
                }
                .padding(.leading, insets.leading + metrics.padding)
                .padding(.trailing, insets.trailing + metrics.padding)
            }
        }
        // On an iPad, a click counts as touch, which hides the arrows. Moving the pointer brings them back.
        .onContinuousHover { phase in
            if case .active = phase { hub.input.noteInput(.keyboardMouse) }
        }
        .accessibilityAddTraits(.isModal)
    }
}

/// A round button on the viewer's material, outside the focus table because the viewer reads the controller itself.
private struct CircleButton: View {
    let systemImage: String
    let label: String
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Image(systemName: systemImage)
                .font(.system(size: 17, weight: .bold))
                .frame(width: 44, height: 44)
                .background(.ultraThinMaterial, in: .circle)
                .overlay { Circle().strokeBorder(Theme.stroke) }
                .contentShape(.circle)
        }
        .buttonStyle(.plain)
        .accessibilityLabel(label)
    }
}
