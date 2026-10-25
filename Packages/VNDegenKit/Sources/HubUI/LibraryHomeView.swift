import SwiftUI
import VNCore

struct LibraryHomeView: View {
    @Environment(Navigator.self) private var nav
    @Environment(\.hubMetrics) private var metrics

    var body: some View {
        let hero = nav.heroGame
        ZStack {
            HeroBackdrop(game: hero)
            ScrollViewReader { proxy in
                ScrollView(.vertical) {
                    VStack(alignment: .leading, spacing: metrics.isShort ? 18 : 30) {
                        HeroStage(game: hero)
                            .frame(height: metrics.stageHeight)
                            .id("stage")
                        ForEach(nav.shelves, id: \.shelf) { shelf in
                            ShelfRow(shelf: shelf.shelf, games: shelf.games)
                        }
                    }
                    .padding(.bottom, 48)
                }
                .scrollIndicators(.hidden)
                // Shelves run to the screen edges. Content is inset by hand with metrics.leading and trailing.
                .ignoresSafeArea(edges: .horizontal)
                .onChange(of: nav.focus) { _, focus in
                    guard nav.focusMovedByController else { return }
                    withAnimation(Theme.focusAnimation) {
                        if case let .tile(shelf, _)? = focus {
                            // Nested scroll views: scroll the page to the shelf, then the shelf to the tile.
                            proxy.scrollTo(shelf.rawValue, anchor: .bottom)
                            proxy.scrollTo(focus, anchor: UnitPoint(x: 0.35, y: 0.5))
                        } else {
                            proxy.scrollTo("stage", anchor: .top)
                        }
                    }
                }
            }
        }
        .overlay(alignment: .top) { TopBar() }
    }
}

/// The focused game's art behind the whole home screen: a sharp hero across the top that fades into a blurred,
/// dimmed wash of the same image, so foreground text stays readable on any art.
struct HeroBackdrop: View {
    @Environment(\.hubMetrics) private var metrics
    let game: Game?

    var body: some View {
        GeometryReader { geo in
            ZStack(alignment: .top) {
                Theme.background
                if let game {
                    ZStack(alignment: .top) {
                        GameArt(game: game, slots: [.hero, .header, .capsule], size: geo.size, blurred: true)
                            .opacity(0.45)
                        GameArt(game: game, slots: [.hero, .header, .capsule],
                                size: CGSize(width: geo.size.width, height: metrics.insets.top + metrics.stageHeight * 1.08))
                            .mask(LinearGradient(stops: [.init(color: .black, location: 0.55), .init(color: .clear, location: 1)],
                                                 startPoint: .top, endPoint: .bottom))
                    }
                    .id(game.id)
                    .transition(.opacity)
                }
                LinearGradient(colors: [Theme.background.opacity(0.85), Theme.background.opacity(0.25), .clear],
                               startPoint: .leading, endPoint: .trailing)
                    .frame(width: geo.size.width * (metrics.isCompact ? 1 : 0.7))
                    .frame(maxWidth: .infinity, alignment: .leading)
                LinearGradient(stops: [.init(color: .clear, location: 0.3), .init(color: Theme.background.opacity(0.92), location: 0.78)],
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
        HStack(spacing: 12) {
            Wordmark()
            Spacer()
            if !nav.visibleGames.isEmpty {
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
        .padding(.horizontal, metrics.padding)
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
    var body: some View {
        HStack(spacing: 8) {
            Image(systemName: "play.fill")
                .font(.system(size: 10, weight: .black))
                .foregroundStyle(.white)
                .frame(width: 24, height: 24)
                .background(Theme.accent.gradient, in: .rect(cornerRadius: 7, style: .continuous))
            Text("VNDEGEN")
                .font(.display(15, weight: .black))
                .tracking(2)
                .foregroundStyle(.white)
        }
        .accessibilityElement(children: .combine)
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

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            SectionLabel(text: shelf.title, systemImage: shelf == .hidden ? "lock.open.fill" : nil)
                .padding(.leading, metrics.leading)
            ScrollView(.horizontal) {
                LazyHStack(spacing: metrics.tileSpacing) {
                    ForEach(games) { game in
                        GameTile(game: game, target: .tile(shelf, game.id))
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
