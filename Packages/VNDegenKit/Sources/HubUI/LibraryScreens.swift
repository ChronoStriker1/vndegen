import SwiftUI
import VNCore
import Metadata
import GameInput
import Lock
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

/// Header shared by the grid and settings screens: back button and a title.
private struct ScreenHeader: View {
    @Environment(\.hubMetrics) private var metrics
    let title: String
    var detail: String?

    var body: some View {
        HStack(spacing: 16) {
            HubButton(target: .back, kind: .icon) { Image(systemName: "chevron.left") }
                .accessibilityLabel("Back")
            Text(title).font(.display(metrics.isCompact ? 24 : 30))
            if let detail {
                Text(detail).font(.system(size: 15, weight: .semibold)).foregroundStyle(Theme.tertiaryText)
            }
            Spacer()
        }
        .padding(.horizontal, metrics.padding)
        .padding(.top, 10)
        #if os(macOS)
        .padding(.top, 18)
        #endif
    }
}

struct LibraryGridView: View {
    @Environment(Navigator.self) private var nav
    @Environment(\.hubMetrics) private var metrics

    var body: some View {
        let games = nav.visibleGames
        let spacing = metrics.tileSpacing
        let available = metrics.fullWidth - 2 * max(metrics.insets.leading, metrics.insets.trailing) - metrics.padding * 2
        let target = metrics.isCompact ? (available - spacing * 2) / 3 : metrics.tileWidth
        let columns = max(2, Int((available + spacing) / (target + spacing)))
        // Stretch tiles so each row runs edge to edge instead of leaving a ragged gap on the right.
        let tile = (available - spacing * CGFloat(columns - 1)) / CGFloat(columns)
        VStack(alignment: .leading, spacing: 8) {
            ScreenHeader(title: "All games", detail: "\(games.count)")
            ScrollViewReader { proxy in
                ScrollView {
                    LazyVGrid(columns: Array(repeating: GridItem(.fixed(tile), spacing: metrics.tileSpacing), count: columns),
                              alignment: .leading, spacing: metrics.tileSpacing + 8) {
                        ForEach(games) { game in
                            GameTile(game: game, target: .gridTile(game.id), width: tile)
                        }
                    }
                    .padding(.horizontal, metrics.padding)
                    .padding(.vertical, 18)
                }
                .scrollIndicators(.hidden)
                .onChange(of: nav.focus) { _, focus in
                    if let focus, focus.gameID != nil {
                        withAnimation(Theme.focusAnimation) { proxy.scrollTo(focus, anchor: .center) }
                    }
                }
            }
        }
        .onChange(of: columns, initial: true) { _, columns in nav.gridColumns = columns }
        .balancedHorizontalSafeArea(metrics)
        .background { Theme.background.ignoresSafeArea() }
    }
}

struct SettingsView: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav
    @Environment(\.hubMetrics) private var metrics
    @AppStorage(TouchControlsMode.storageKey) private var touchMode = TouchControlsMode.gamepad
    @AppStorage(PlayOrientation.storageKey) private var orientation = PlayOrientation.landscape
    @State private var page: Page?

    /// MetadataSettingsView and LockSettingsView are whole forms, so they open in their own sheet.
    private enum Page: String, Identifiable {
        case metadata, lock
        var id: String { rawValue }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            ScreenHeader(title: "Settings")
            Form {
                Section {
                    LabeledContent("Games folder") {
                        Text(Self.displayPath(hub.library.paths.gamesRoot))
                            .font(.system(size: 13, design: .monospaced))
                            .textSelection(.enabled)
                    }
                    Button(LibraryFolder.openTitle, systemImage: "folder") { LibraryFolder.open(hub.library.paths.gamesRoot) }
                    Button("Rescan library", systemImage: "arrow.clockwise") { Task { await hub.refreshLibrary() } }
                } header: {
                    Text("Library")
                } footer: {
                    Text(LibraryFolder.instructions)
                }

                Section("Sources and privacy") {
                    pageRow("Metadata sources", systemImage: "text.magnifyingglass",
                            value: hub.metadata.activeSources.map(\.displayName).formatted(.list(type: .and)), page: .metadata)
                    pageRow("Hidden games", systemImage: "lock",
                            value: hub.lock.hasPasscode ? "Passcode set" : "No passcode", page: .lock)
                }

                Section {
                    Picker("Touch controls", selection: $touchMode) {
                        ForEach(TouchControlsMode.allCases, id: \.self) { Text($0.title).tag($0) }
                    }
                    #if os(iOS)
                    Picker("In-game orientation", selection: $orientation) {
                        ForEach(PlayOrientation.allCases, id: \.self) { Text($0.title).tag($0) }
                    }
                    #endif
                } header: {
                    Text("Controls")
                } footer: {
                    #if os(iOS)
                    Text("Touch controls show over a game when you play by touch and no controller is connected. In-game orientation applies while a game plays, and each game page can override it.")
                    #else
                    Text("Shown over a game when you play by touch and no controller is connected.")
                    #endif
                }

                Section {
                    LabeledContent("Version", value: Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "")
                }
            }
            .formStyle(.grouped)
            .scrollContentBackground(.hidden)
            .frame(maxWidth: 760)
            .frame(maxWidth: .infinity)
        }
        .balancedHorizontalSafeArea(metrics)
        .background { Theme.background.ignoresSafeArea() }
        .sheet(item: $page) { page in
            Group {
                switch page {
                case .metadata:
                    SheetScaffold(title: "Metadata sources", escapeCloses: true, onClose: { self.page = nil }) {
                        MetadataSettingsView(service: hub.metadata).scrollContentBackground(.hidden)
                    }
                case .lock:
                    SheetScaffold(title: "Hidden games", onClose: { self.page = nil }) {
                        LockSettingsView(lock: hub.lock).scrollContentBackground(.hidden)
                    }
                }
            }
            .environment(nav)
            .presentationDetents([.large])
            #if os(macOS)
            .frame(minWidth: 520, minHeight: 560)
            #endif
        }
    }

    private func pageRow(_ title: String, systemImage: String, value: String, page: Page) -> some View {
        Button { self.page = page } label: {
            HStack {
                Label(title, systemImage: systemImage).foregroundStyle(.white)
                Spacer()
                Text(value).foregroundStyle(Theme.secondaryText).lineLimit(1)
                Image(systemName: "chevron.right").font(.footnote.weight(.semibold)).foregroundStyle(Theme.tertiaryText)
            }
            .contentShape(.rect)
        }
        .buttonStyle(.plain)
    }

    static func displayPath(_ url: URL) -> String {
        let path = url.path
        #if os(macOS)
        let home = FileManager.default.homeDirectoryForCurrentUser.path
        return path.hasPrefix(home) ? "~" + path.dropFirst(home.count) : path
        #else
        return path.components(separatedBy: "/Documents/").last.map { "On My \(UIDevice.current.localizedModel) › VNDegen › \($0)" } ?? path
        #endif
    }
}

extension TouchControlsMode {
    static let storageKey = "touchControlsMode"

    var title: String {
        switch self {
        case .gamepad: "Gamepad"
        case .mouse: "Mouse"
        case .hidden: "Hidden"
        }
    }
}

/// Opening the games folder in the system file browser, and how to add games there.
enum LibraryFolder {
    #if os(macOS)
    static let openTitle = "Show in Finder"
    static let instructions = "Copy each game into its own folder inside ~/Documents/VNDegen/Games, then rescan."
    static func open(_ url: URL) {
        try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        NSWorkspace.shared.activateFileViewerSelecting([url])
    }
    #else
    static let openTitle = "Open in Files"
    static let instructions = "In the Files app, open On My \(UIDevice.current.localizedModel), then VNDegen, then Games. Copy each game into its own folder there."
    static func open(_ url: URL) {
        try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        // The Files app opens a folder in an app's container through the shareddocuments scheme.
        if let files = URL(string: "shareddocuments://\(url.path)") { UIApplication.shared.open(files) }
    }
    #endif
}

struct EmptyLibraryView: View {
    @Environment(Hub.self) private var hub
    @Environment(\.hubMetrics) private var metrics
    @State private var scanning = false

    private var steps: [String] {
        #if os(macOS)
        ["Open the Games folder in Finder.", "Copy each game into its own folder, for example Games/No Socks RPG.", "Come back here. VNDegen finds the engine and adds the game."]
        #else
        let device = UIDevice.current.localizedModel
        return ["Open the Files app and go to On My \(device), then VNDegen, then Games.", "Copy each game into its own folder, for example Games/No Socks RPG.", "Come back here. VNDegen finds the engine and adds the game."]
        #endif
    }

    var body: some View {
        let layout = metrics.isCompact || metrics.isShort ? AnyLayout(VStackLayout(spacing: 28)) : AnyLayout(HStackLayout(spacing: 56))
        ScrollView {
            layout {
                PosterStack()
                    .frame(width: metrics.isShort ? 180 : 240, height: metrics.isShort ? 170 : 240)
                VStack(alignment: .leading, spacing: 20) {
                    Text("Add your first game").font(.display(metrics.isCompact ? 28 : 36))
                    VStack(alignment: .leading, spacing: 14) {
                        ForEach(Array(steps.enumerated()), id: \.offset) { index, step in
                            HStack(alignment: .firstTextBaseline, spacing: 12) {
                                Text("\(index + 1)")
                                    .font(.system(size: 13, weight: .heavy).width(.expanded))
                                    .frame(width: 26, height: 26)
                                    .background(Theme.accent.opacity(0.9), in: .circle)
                                Text(step).font(.system(size: 16)).foregroundStyle(.white.opacity(0.86))
                                    .fixedSize(horizontal: false, vertical: true)
                            }
                        }
                    }
                    Text("RPG Maker MV and MZ games are supported today.")
                        .font(.system(size: 14))
                        .foregroundStyle(Theme.tertiaryText)
                    HStack(spacing: 12) {
                        Button { LibraryFolder.open(hub.library.paths.gamesRoot) } label: {
                            Label(LibraryFolder.openTitle, systemImage: "folder.fill")
                        }
                        .buttonStyle(HubButtonStyle(kind: .primary, focused: false))
                        Button {
                            scanning = true
                            Task { await hub.refreshLibrary(); scanning = false }
                        } label: {
                            Label("Rescan", systemImage: "arrow.clockwise")
                        }
                        .buttonStyle(HubButtonStyle(kind: .secondary, focused: false))
                        .disabled(scanning)
                    }
                    .padding(.top, 6)
                }
                .frame(maxWidth: 520, alignment: .leading)
            }
            .padding(.horizontal, metrics.padding)
            .padding(.vertical, 90)
            .frame(maxWidth: .infinity, minHeight: metrics.size.height)
        }
        .scrollIndicators(.hidden)
        .balancedHorizontalSafeArea(metrics)
        .refreshable { await hub.refreshLibrary() }
        .background {
            RadialGradient(colors: [Theme.accent.opacity(0.22), Theme.background], center: .init(x: 0.3, y: 0.35),
                           startRadius: 0, endRadius: max(metrics.size.width, metrics.size.height) * 0.7)
                .ignoresSafeArea()
        }
    }
}

/// Three fanned generated posters, standing in for the library the user is about to add.
private struct PosterStack: View {
    var body: some View {
        GeometryReader { geo in
            let width = geo.size.width * 0.5
            ZStack {
                ForEach(Array(["Your Visual Novel", "Your RPG", "Your Game"].enumerated()), id: \.offset) { index, title in
                    PlaceholderArt(title: title)
                        .frame(width: width, height: width * 1.5)
                        .clipShape(.rect(cornerRadius: 10, style: .continuous))
                        .overlay { RoundedRectangle(cornerRadius: 10, style: .continuous).strokeBorder(.white.opacity(0.15)) }
                        .shadow(color: .black.opacity(0.5), radius: 16, y: 8)
                        .rotationEffect(.degrees(Double(index - 1) * 12), anchor: .bottom)
                        .offset(x: CGFloat(index - 1) * width * 0.42, y: index == 1 ? -8 : 0)
                        .zIndex(index == 1 ? 1 : 0)
                }
            }
            .frame(width: geo.size.width, height: geo.size.height)
        }
        .accessibilityHidden(true)
    }
}
