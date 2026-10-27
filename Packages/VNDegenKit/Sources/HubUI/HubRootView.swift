import SwiftUI
import VNCore
import SaveSync
import ArchiveImport

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
            if phase == .background {
                nav.relock()
                nav.discardInboxCopies()
            }
            // Picks up games and archives the user copied in with Files or Finder meanwhile.
            if phase == .active, case .library = hub.screen { Task { await hub.refreshLibrary() } }
        }
        .task { ArchiveImporter.discardStaleDropCopies() }
        // An archive opened in VNDegen from Files, Finder, or a share sheet. The app declares zip, RAR, and 7z.
        .onOpenURL { url in
            if url.isFileURL { nav.openImport([url]) }
        }
    }
}

private struct LibraryContainer: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav
    @State private var dropTargeted = false

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
        .overlay(alignment: .bottom) {
            if nav.checkingSaves != nil {
                Label("Checking for newer saves…", systemImage: "arrow.triangle.2.circlepath")
                    .font(.callout.weight(.semibold))
                    .padding(.horizontal, 18)
                    .padding(.vertical, 10)
                    .background(.ultraThinMaterial, in: .capsule)
                    .padding(.bottom, 32)
                    .transition(.opacity)
            }
        }
        .animation(.smooth(duration: 0.2), value: nav.checkingSaves)
        .sheet(item: $nav.sheet) { sheet in
            sheetContent(sheet)
                .environment(hub)
                .environment(nav)
        }
        .alert("Can't launch", isPresented: Binding(get: { hub.launchError != nil }, set: { if !$0 { hub.launchError = nil } })) {
            Button("OK") {}
        } message: {
            // The alert's button shows no controller focus, so controller players get the buttons spelled out.
            Text((hub.launchError ?? "") + (hub.input.lastInputKind == .gamepad ? " Press A or B to close." : ""))
        }
        .onAppear {
            nav.attachInput()
            nav.revalidate()
        }
        .onChange(of: hub.library.games) { nav.revalidate() }
        .onChange(of: hub.localArchives) { nav.revalidate() }
        .onChange(of: hub.lock.isUnlocked) { nav.revalidate() }
        // Archives dropped anywhere on the library open the import sheet with them chosen.
        .onArchiveDrop(isTargeted: { dropTargeted = $0 && nav.sheet == nil }) { nav.openImport($0) }
        .overlay {
            if dropTargeted {
                // Shown while dragging only. The drop itself lands on the container, so this one takes no hits.
                ArchiveDropTarget { _ in }
                    .frame(maxWidth: 460)
                    .padding(40)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .background(Theme.background.opacity(0.75))
                    .allowsHitTesting(false)
                    .transition(.opacity)
            }
        }
        .animation(.easeOut(duration: 0.15), value: dropTargeted)
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
        case let .variant(id):
            if let game = hub.library.game(id), let variants = nav.variants(id) {
                ChoiceSheet(sheet: sheet, title: variants.title, subtitle: hub.displayTitle(game),
                            footnote: "An advanced option. Automatic picks the right one for almost every game.")
            }
        case let .orientation(id):
            if let game = hub.library.game(id) {
                ChoiceSheet(sheet: sheet, title: "In-game orientation", subtitle: hub.displayTitle(game),
                            footnote: "Landscape keeps either landscape side and follows how you hold the device. Some mobile-style games are made for portrait.")
            }
        case let .controlMethod(id):
            if let game = hub.library.game(id) {
                ChoiceSheet(sheet: sheet, title: "Controls", subtitle: hub.displayTitle(game),
                            footnote: "The VNDegen menu switches the control method while you play, and the change stays with this game.")
            }
        case let .mapping(editor):
            ButtonMappingSheet(editor: editor)
        case let .artwork(editor):
            if let game = hub.library.game(editor.gameID) { ArtworkEditorSheet(game: game, editor: editor) }
        case let .metadata(search):
            if let game = hub.library.game(search.gameID) { MetadataSearchSheet(game: game, search: search) }
        case let .saves(model):
            if let game = hub.library.game(model.game) {
                SheetScaffold(title: "Saves", subtitle: hub.displayTitle(game), escapeCloses: true) {
                    GameSavesView(model: model).scrollContentBackground(.hidden)
                }
                .presentationDetents([.large])
                #if os(macOS)
                .frame(minWidth: 520, minHeight: 560)
                #endif
            }
        case let .launchSaves(model):
            if let game = hub.library.game(model.game) {
                SheetScaffold(title: "Newer saves", subtitle: hub.displayTitle(game), escapeCloses: false) {
                    LaunchSavesView(model: model).scrollContentBackground(.hidden)
                }
                .presentationDetents([.medium, .large])
                .interactiveDismissDisabled()
                #if os(macOS)
                .frame(minWidth: 480, minHeight: 380)
                #endif
            }
        case let .settingsPage(page):
            SettingsPageSheet(page: page)
        case let .importGame(model, initialSelection):
            ArchiveImportView(model: model, initialSelection: initialSelection) { nav.finishImport($0) }
        case let .license(request):
            RuntimeLicenseSheet(request: request)
                .presentationDetents([.large])
                .interactiveDismissDisabled()
                #if os(macOS)
                .frame(minWidth: 560, minHeight: 600)
                #endif
        }
    }
}
