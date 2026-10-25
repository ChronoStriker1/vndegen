import SwiftUI
import VNCore
import SaveSync

/// The Big Picture-style hub: library, game pages, settings, and the player.
public struct HubRootView: View {
    @Bindable var hub: Hub
    // Owned here so the route and focus survive size changes, such as folding an iPhone Duo, and play sessions.
    @State private var nav: Navigator
    @Environment(\.scenePhase) private var scenePhase

    public init(hub: Hub) {
        self.hub = hub
        _nav = State(initialValue: Navigator(hub: hub))
    }

    public var body: some View {
        GeometryReader { geo in
            Group {
                switch hub.screen {
                case .library: LibraryContainer()
                case let .playing(_, session): PlayerView(hub: hub, session: session)
                }
            }
            .environment(\.hubMetrics, HubMetrics(size: geo.size, insets: geo.safeAreaInsets))
        }
        .environment(hub)
        .environment(nav)
        .foregroundStyle(.white)
        .tint(Theme.accent)
        .preferredColorScheme(.dark)
        .background(Theme.background)
        .onChange(of: scenePhase) { _, phase in
            if phase == .background { nav.relock() }
        }
    }
}

private struct LibraryContainer: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav

    var body: some View {
        @Bindable var nav = nav
        ZStack {
            switch nav.route {
            case nil:
                if nav.visibleGames.isEmpty {
                    EmptyLibraryView()
                        .overlay(alignment: .top) { TopBar() }
                        .transition(.opacity)
                } else {
                    LibraryHomeView().transition(.opacity)
                }
            case .grid:
                LibraryGridView().transition(.opacity.combined(with: .scale(scale: 1.03)))
            case let .game(id):
                if let game = hub.library.game(id) {
                    GamePageView(game: game).transition(.opacity.combined(with: .scale(scale: 1.03)))
                }
            case .settings:
                SettingsView().transition(.opacity.combined(with: .move(edge: .trailing)))
            }
        }
        .animation(.smooth(duration: 0.35), value: nav.path)
        .sheet(item: $nav.sheet) { sheet in
            sheetContent(sheet)
                .environment(hub)
                .environment(nav)
        }
        .alert("Can't launch", isPresented: Binding(get: { hub.launchError != nil }, set: { if !$0 { hub.launchError = nil } })) {
            Button("OK") {}
        } message: {
            Text(hub.launchError ?? "")
        }
        .onAppear {
            nav.attachInput()
            nav.revalidate()
        }
        .onChange(of: hub.library.games) { nav.revalidate() }
        .onChange(of: hub.lock.isUnlocked) { nav.revalidate() }
    }

    @ViewBuilder
    private func sheetContent(_ sheet: Navigator.Sheet) -> some View {
        switch sheet {
        case .unlock:
            // UnlockView brings its own backdrop and Cancel button, and calls onDone either way.
            nav.unlockView
                .presentationDetents([.large])
                #if os(macOS)
                .frame(minWidth: 560, minHeight: 600)
                #endif
        case let .engine(id):
            if let game = hub.library.game(id) {
                ChoiceSheet(sheet: sheet, title: "Engine", subtitle: hub.displayTitle(game),
                            footnote: "Only engines that run on this device are listed. A manual choice wins over detection until you switch back to Automatic.")
            }
        case let .orientation(id):
            if let game = hub.library.game(id) {
                ChoiceSheet(sheet: sheet, title: "In-game orientation", subtitle: hub.displayTitle(game),
                            footnote: "Landscape keeps either landscape side and follows how you hold the device. Some mobile-style games are made for portrait.")
            }
        case let .artwork(id):
            if let game = hub.library.game(id) { ArtworkEditorSheet(game: game) }
        case let .metadata(id):
            if let game = hub.library.game(id) { MetadataSearchSheet(game: game) }
        case let .saves(id):
            if let game = hub.library.game(id) {
                SheetScaffold(title: "Saves", subtitle: hub.displayTitle(game), escapeCloses: true) {
                    GameSavesView(service: hub.saveSync, game: id).scrollContentBackground(.hidden)
                }
                .presentationDetents([.large])
                #if os(macOS)
                .frame(minWidth: 520, minHeight: 560)
                #endif
            }
        case let .settingsPage(page):
            SettingsPageSheet(page: page)
        }
    }
}
