import SwiftUI
import VNCore

struct LibraryHomeView: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav
    @Environment(\.hubMetrics) private var metrics

    /// Room above the first row for the top bar, which floats over the content, in the layouts without a hero.
    private var barClearance: CGFloat {
        #if os(macOS)
        88
        #else
        70
        #endif
    }

    var body: some View {
        let hero = nav.heroGame
        let layout = nav.layout
        ZStack {
            switch layout {
            case .featured: HeroBackdrop(game: hero)
            case .capsules: HeroBackdrop(game: hero, sharp: false)
            case .headers: Theme.background.ignoresSafeArea()
            }
            ScrollViewReader { proxy in
                ScrollView(.vertical) {
                    VStack(alignment: .leading, spacing: metrics.isShort ? 18 : 30) {
                        if layout == .featured {
                            HeroStage(game: hero)
                                .frame(height: metrics.stageHeight)
                                .id("stage")
                        } else {
                            Color.clear.frame(height: barClearance).id("stage")
                        }
                        if !hub.localArchives.isEmpty {
                            ReadyToImportRow(archives: hub.localArchives)
                        }
                        ForEach(nav.shelves, id: \.shelf) { shelf in
                            if layout == .headers {
                                HeaderShelf(shelf: shelf.shelf, games: shelf.games)
                            } else {
                                ShelfRow(shelf: shelf.shelf, games: shelf.games,
                                         tileWidth: layout == .capsules ? metrics.largeTileWidth : metrics.tileWidth)
                            }
                        }
                    }
                    .padding(.bottom, 48)
                }
                .scrollIndicators(.hidden)
                // Shelves run to the screen edges. Content is inset by hand with metrics.leading and trailing.
                .ignoresSafeArea(edges: .horizontal)
                .onChange(of: nav.focus) { _, focus in
                    guard nav.focusMovedByController else { return }
                    withAnimation(Theme.focusAnimation) { reveal(focus, layout, proxy) }
                }
                // Back from a game page rebuilds this screen at the top, while focus is already on the game's tile.
                .onAppear {
                    guard nav.focusMovedByController, let focus = nav.focus else { return }
                    Task { reveal(focus, layout, proxy) }
                }
            }
            // A new layout starts at the top rather than at the old one's scroll offset.
            .id(layout)
            .transition(.opacity)
        }
        .animation(.smooth(duration: 0.3), value: layout)
        .onChange(of: metrics.headerColumns, initial: true) { _, columns in nav.headerColumns = columns }
        .overlay(alignment: .top) { TopBar() }
    }

    private func reveal(_ focus: Navigator.Target?, _ layout: LibraryLayout, _ proxy: ScrollViewProxy) {
        let row: String? = switch focus {
        case let .tile(shelf, _)?: shelf.rawValue
        case .importArchive?: Navigator.archivesRowID
        default: nil
        }
        if layout == .headers, case .tile? = focus {
            proxy.scrollTo(focus, anchor: .center)
        } else if let row {
            // Nested scroll views: scroll the page to the row, then the row to the focused item.
            proxy.scrollTo(row, anchor: .bottom)
            proxy.scrollTo(focus, anchor: UnitPoint(x: 0.35, y: 0.5))
        } else {
            proxy.scrollTo("stage", anchor: .top)
        }
    }
}

/// The focused game's art behind the whole home screen: a sharp hero across the top that fades into a blurred,
/// dimmed wash of the same image, so foreground text stays readable on any art.
struct HeroBackdrop: View {
    @Environment(\.hubMetrics) private var metrics
    let game: Game?
    /// Without the sharp hero, only the blurred wash shows, for layouts that put art at the top instead.
    var sharp = true

    var body: some View {
        GeometryReader { geo in
            ZStack(alignment: .top) {
                Theme.background
                if let game {
                    ZStack(alignment: .top) {
                        GameArt(game: game, slots: [.hero, .header, .capsule], size: geo.size, blurred: true)
                            .opacity(0.45)
                        if sharp {
                            GameArt(game: game, slots: [.hero, .header, .capsule],
                                    size: CGSize(width: geo.size.width, height: metrics.insets.top + metrics.stageHeight * 1.08))
                                .mask(LinearGradient(stops: [.init(color: .black, location: 0.55), .init(color: .clear, location: 1)],
                                                     startPoint: .top, endPoint: .bottom))
                        }
                    }
                    .id(game.id)
                    .transition(.opacity)
                }
                // Holds dark across the text column and fades out past it, so bright art stays bright on the right.
                LinearGradient(stops: [.init(color: Theme.background.opacity(0.9), location: 0),
                                       .init(color: Theme.background.opacity(0.75), location: 0.4),
                                       .init(color: Theme.background.opacity(0.3), location: 0.72),
                                       .init(color: .clear, location: 1)],
                               startPoint: .leading, endPoint: .trailing)
                    .frame(width: geo.size.width * (metrics.isCompact ? 1 : 0.75))
                    .frame(maxWidth: .infinity, alignment: .leading)
                LinearGradient(stops: [.init(color: .clear, location: 0.2),
                                       .init(color: Theme.background.opacity(0.45), location: 0.5),
                                       .init(color: Theme.background.opacity(0.92), location: 0.78)],
                               startPoint: .top, endPoint: .bottom)
            }
            .frame(width: geo.size.width, height: geo.size.height)
        }
        .animation(Theme.heroAnimation, value: game?.id)
        .ignoresSafeArea()
    }
}

struct TopBar: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav
    @Environment(\.hubMetrics) private var metrics

    var body: some View {
        HStack(spacing: metrics.isCompact ? 8 : 12) {
            // A narrow iPad window has no room for the name beside the buttons, and less still beside its window controls.
            ViewThatFits(in: .horizontal) {
                Wordmark()
                Wordmark(showsName: false)
                // The narrowest windows keep their room for the buttons.
                Color.clear.frame(width: 0, height: 0)
            }
            .layoutPriority(1)
            // The stack's own spacing keeps the name off the buttons, so it may use all the room up to them.
            Spacer(minLength: 0)
            // The empty library has its own, larger Add game button.
            if !nav.visibleGames.isEmpty {
                HubButton(target: .addGame, kind: .icon) { Image(systemName: "plus") }
                    .accessibilityLabel("Add game")
                // Shows the current layout. Pressing it moves to the next.
                HubButton(target: .layoutButton, kind: .icon) { Image(systemName: nav.layout.systemImage) }
                    .accessibilityLabel("Library view")
                    .accessibilityValue(nav.layout.title)
                HubButton(target: .gridButton, kind: .icon) { Image(systemName: "square.grid.2x2.fill") }
                    .accessibilityLabel("All games")
            }
            HubButton(target: .lockButton, kind: .icon) {
                Image(systemName: hub.lock.isUnlocked ? "lock.open.fill" : "lock.fill")
            }
            .accessibilityLabel(hub.lock.isUnlocked ? "Hide hidden games" : "Show hidden games")
            HubButton(target: .settingsButton, kind: .icon) { Image(systemName: "gearshape.fill") }
                .accessibilityLabel("Settings")
        }
        .padding(.leading, metrics.topRowPadding(.leading))
        .padding(.trailing, metrics.topRowPadding(.trailing))
        .padding(.top, 10)
        #if os(macOS)
        .padding(.top, 18)
        #endif
        .padding(.bottom, 28)
        .background {
            // Content scrolls under the bar, so fade it out before it meets the buttons.
            LinearGradient(colors: [Theme.background.opacity(0.9), Theme.background.opacity(0.6), .clear], startPoint: .top, endPoint: .bottom)
                .ignoresSafeArea(edges: [.top, .horizontal])
        }
    }
}

struct Wordmark: View {
    var showsName = true

    var body: some View {
        HStack(spacing: 8) {
            Image(systemName: "play.fill")
                .font(.system(size: 10, weight: .black))
                .foregroundStyle(.white)
                .frame(width: 24, height: 24)
                .background(Theme.accent.gradient, in: .rect(cornerRadius: 7, style: .continuous))
            if showsName {
                Text("VNDEGEN")
                    .font(.display(15, weight: .black))
                    .tracking(2)
                    .foregroundStyle(.white)
                    .fixedSize()
            }
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("VNDegen")
    }
}

private struct HeroStage: View {
    @Environment(Hub.self) private var hub
    @Environment(\.hubMetrics) private var metrics
    let game: Game?

    var body: some View {
        VStack(alignment: .leading, spacing: metrics.isShort ? 10 : 16) {
            Spacer(minLength: 64)
            if let game {
                VStack(alignment: .leading, spacing: metrics.isShort ? 10 : 16) {
                    GameLogo(game: game, maxSize: metrics.logoMaxSize)
                    HeroFacts(game: game)
                    if !metrics.isShort, !metrics.isCompact, let summary = game.metadata?.summary {
                        Text(summary)
                            .font(.system(size: 15))
                            .foregroundStyle(Theme.secondaryText)
                            .lineLimit(2)
                            .frame(maxWidth: 520, alignment: .leading)
                    }
                    HStack(spacing: 12) {
                        HubButton(target: .play, kind: .primary) {
                            Label("Play", systemImage: "play.fill")
                        }
                        HubButton(target: .details, kind: .secondary) {
                            Label("Details", systemImage: "info.circle")
                        }
                    }
                }
                .id(game.id)
                .transition(.asymmetric(insertion: .opacity.combined(with: .offset(x: 18)), removal: .opacity))
            }
        }
        .animation(Theme.heroAnimation, value: game?.id)
        .padding(.leading, metrics.leading)
        .padding(.trailing, metrics.trailing)
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

/// Engine, playtime, and last played on one line.
struct HeroFacts: View {
    @Environment(Hub.self) private var hub
    let game: Game

    var body: some View {
        HStack(spacing: 10) {
            if let engine = hub.library.plugin(for: game.engine) {
                Tag(text: engine.descriptor.shortName, prominent: true)
            }
            Group {
                if let lastPlayed = game.lastPlayedAt {
                    Text("Played \(lastPlayed.formatted(.relative(presentation: .named)))")
                } else {
                    Text("Not played yet")
                }
                if let playTime = game.playTime.playTimeText {
                    Text("·")
                    Text(playTime)
                }
            }
            .font(.system(size: 14, weight: .medium))
            .foregroundStyle(Theme.secondaryText)
            .lineLimit(1)
        }
    }
}

private struct ShelfRow: View {
    @Environment(\.hubMetrics) private var metrics
    let shelf: Navigator.Shelf
    let games: [Game]
    let tileWidth: CGFloat

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            SectionLabel(text: shelf.title, systemImage: shelf == .hidden ? "lock.open.fill" : nil)
                .padding(.leading, metrics.leading)
            ScrollView(.horizontal) {
                LazyHStack(spacing: metrics.tileSpacing) {
                    ForEach(games) { game in
                        GameTile(game: game, target: .tile(shelf, game.id), width: tileWidth)
                    }
                }
                .padding(.leading, metrics.leading)
                .padding(.trailing, metrics.trailing)
                // Room for the focused tile's scale and glow.
                .padding(.vertical, 14)
            }
            .scrollIndicators(.hidden)
            .scrollClipDisabled()
        }
        .id(shelf.rawValue)
    }
}

/// A shelf as header art stacked in columns, for the header list layout.
private struct HeaderShelf: View {
    @Environment(\.hubMetrics) private var metrics
    let shelf: Navigator.Shelf
    let games: [Game]

    var body: some View {
        let width = metrics.headerWidth
        VStack(alignment: .leading, spacing: 12) {
            SectionLabel(text: shelf.title, systemImage: shelf == .hidden ? "lock.open.fill" : nil)
            LazyVGrid(columns: Array(repeating: GridItem(.fixed(width), spacing: metrics.tileSpacing), count: metrics.headerColumns),
                      alignment: .leading, spacing: metrics.tileSpacing) {
                ForEach(games) { game in
                    HeaderTile(game: game, target: .tile(shelf, game.id), width: width)
                }
            }
        }
        .padding(.leading, metrics.leading)
        .padding(.trailing, metrics.trailing)
        .id(shelf.rawValue)
    }
}
