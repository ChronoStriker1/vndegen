import SwiftUI
import ControllerUI
import FontPack
import VNCore
import Metadata
import GameInput
import Lock
import SaveSync
import UniformTypeIdentifiers
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
        .padding(.leading, metrics.topRowPadding(.leading))
        .padding(.trailing, metrics.topRowPadding(.trailing))
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
                    guard nav.focusMovedByController else { return }
                    if let focus, focus.gameID != nil {
                        withAnimation(Theme.focusAnimation) { proxy.scrollTo(focus, anchor: .center) }
                    }
                }
                .onChange(of: nav.revealRequest) { _, target in
                    guard let target else { return }
                    nav.revealRequest = nil
                    withAnimation(Theme.focusAnimation) { proxy.scrollTo(target, anchor: .center) }
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
    #if os(iOS)
    @AppStorage(ControlMethod.storageKey) private var controlMethod = ControlMethod.automatic
    @AppStorage(PlayOrientation.storageKey) private var orientation = PlayOrientation.landscape
    #endif

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            ScreenHeader(title: "Settings")
            ScrollViewReader { proxy in
                Form {
                    Section {
                        builtInFolderRow
                        ForEach(hub.library.folders) { folderRow($0) }
                        Button("Add folder…", systemImage: "folder.badge.plus") { nav.select(.addLibraryFolder) }
                            .focusRow(.addLibraryFolder, nav)
                            .fileImporter(isPresented: Binding(get: { nav.pickingLibraryFolder }, set: { nav.pickingLibraryFolder = $0 }),
                                          allowedContentTypes: [.folder]) { nav.addLibraryFolder($0) }
                        Button("Rescan library", systemImage: "arrow.clockwise") { nav.select(.rescan) }
                            .disabled(nav.rescanning)
                            .focusRow(.rescan, nav)
                    } header: {
                        Text("Library folders")
                    } footer: {
                        Text(FileBrowser.instructions + " " + FileBrowser.addedFolders)
                    }

                    Section("Sources and privacy") {
                        pageRow("Metadata sources", systemImage: "text.magnifyingglass",
                                value: hub.metadata.activeSources.map(\.displayName).formatted(.list(type: .and)), page: .metadata)
                        pageRow("Hidden games", systemImage: "lock",
                                value: hub.lock.hasPasscode ? "Passcode set" : "No passcode", page: .lock)
                    }

                    Section {
                        pageRow("Ludusavi backups", systemImage: "externaldrive", value: savesSummary, page: .saves)
                    } header: {
                        Text("Saves")
                    } footer: {
                        Text("Restore and back up game saves through Ludusavi. Each game's page lists its backups under Saves.")
                    }

                    #if os(iOS)
                    Section {
                        Picker("Control method", selection: Binding(get: { controlMethod.kind }, set: { nav.setControlMethod(ControlMethod($0)) })) {
                            ForEach(ControlMethod.Kind.allCases, id: \.self) { Text($0.title).tag($0) }
                        }
                        .focusRow(.controlMethod, nav)
                        if controlMethod.kind == .controller {
                            Toggle("Mouse", isOn: Binding(get: { controlMethod.mouse != nil }, set: { _ in nav.setControlMethod(controlMethod.togglingMouse) }))
                                .focusRow(.controllerMouse, nav)
                        }
                        if let mouse = controlMethod.mouse {
                            Picker("Mouse style", selection: Binding(get: { mouse }, set: { nav.setControlMethod(controlMethod.with(mouse: $0)) })) {
                                ForEach(MouseStyle.allCases, id: \.self) { Text($0.title).tag($0) }
                            }
                            .focusRow(.mouseStyle, nav)
                        }
                        Picker("In-game orientation", selection: $orientation) {
                            ForEach(PlayOrientation.allCases, id: \.self) { Text($0.title).tag($0) }
                        }
                        .focusRow(.playOrientation, nav)
                    } header: {
                        Text("Controls")
                    } footer: {
                        Text("Automatic shows the touch gamepad when you play by touch with no controller connected. Mouse with the touch style clicks where you tap, and the cursor style moves a pointer like a trackpad. Controller shows no on-screen controls, and the game ignores touches unless Mouse is on. Each game page can override the control method and orientation, and the VNDegen menu switches the control method while you play.")
                    }
                    #endif

                    Section {
                        pageRow("Plugins", systemImage: "puzzlepiece.extension", value: "\(hub.library.engines.count) built in", page: .plugins)
                    } header: {
                        Text("Engines")
                    } footer: {
                        Text("Each engine plugin, the versions it covers, what runs its games, its license, and how many of your games use it.")
                    }

                    Section {
                        ForEach(FontPack.packages) { fontRow($0) }
                    } header: {
                        Text("Fonts")
                    } footer: {
                        Text("Games ask for Windows fonts such as MS Gothic and Arial by name. VNDegen includes free fonts with the same measurements for Western and Japanese text. Chinese and Korean games need these downloads from Adobe's Source Han Sans, under the SIL Open Font License. Games started after a download use it.")
                    }

                    Section {
                        soundFontRow
                        if nav.soundFontName != nil {
                            Button("Use the built-in SoundFont", systemImage: "arrow.uturn.backward") { nav.select(.soundFontReset) }
                                .focusRow(.soundFontReset, nav)
                        }
                    } header: {
                        Text("MIDI music")
                    } footer: {
                        Text("RPG Maker XP, VX and VX Ace, Wolf RPG Editor and AliceSoft System 3 games play their MIDI music through a General MIDI SoundFont. Choose an .sf2 or .sf3 file to use instead of the built-in FluidR3 Mono; VNDegen keeps a copy of it. If it can't be read, the built-in font plays. Games started after a change use it.")
                    }

                    Section {
                        LabeledContent("Version", value: Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "")
                        pageRow("Licenses", systemImage: "doc.text", value: "", page: .licenses)
                    }
                }
                .formStyle(.grouped)
                .scrollContentBackground(.hidden)
                .frame(maxWidth: 760)
                .frame(maxWidth: .infinity)
                .onChange(of: nav.focus) { _, focus in
                    guard nav.focusMovedByController, let focus else { return }
                    withAnimation(Theme.focusAnimation) { proxy.scrollTo(focus, anchor: .center) }
                }
            }
        }
        .balancedHorizontalSafeArea(metrics)
        .background { Theme.background.ignoresSafeArea() }
    }

    /// The built-in games folder, which can't be removed.
    private var builtInFolderRow: some View {
        let root = hub.library.paths.gamesRoot
        return HStack(spacing: 12) {
            folderSummary(name: root.lastPathComponent, path: Self.displayPath(root),
                          status: "Built in · \(gameCount(nil))")
            Spacer()
            Button(FileBrowser.openTitle) { nav.select(.openFolder) }
                .buttonStyle(.borderless)
                .focusRow(.openFolder, nav)
        }
    }

    /// A folder the player added: where it is, how many games it holds or why it can't be reached, and its buttons.
    private func folderRow(_ folder: LibraryFolder) -> some View {
        let problem = hub.library.folderProblems[folder.id]
        return HStack(spacing: 12) {
            folderSummary(name: folder.name, path: Self.displayPath(folder.url),
                          status: problem.map { "Unavailable. \($0) \(gameCount(folder.id)) still listed." } ?? gameCount(folder.id),
                          unavailable: problem != nil)
            Spacer()
            Button(FileBrowser.openTitle) { nav.select(.showLibraryFolder(folder.id)) }
                .buttonStyle(.borderless)
                .disabled(problem != nil)
                .focusRow(.showLibraryFolder(folder.id), nav)
            Button("Remove", role: .destructive) { nav.select(.removeLibraryFolder(folder.id)) }
                .buttonStyle(.borderless)
                .focusRow(.removeLibraryFolder(folder.id), nav)
        }
    }

    private func folderSummary(name: String, path: String, status: String, unavailable: Bool = false) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Label(name, systemImage: unavailable ? "externaldrive.badge.exclamationmark" : "folder")
                .foregroundStyle(.white)
            Text(path)
                .font(.system(size: 12, design: .monospaced))
                .foregroundStyle(Theme.secondaryText)
                .textSelection(.enabled)
                .lineLimit(2)
                .truncationMode(.middle)
            Text(status)
                .font(.footnote)
                .foregroundStyle(unavailable ? Theme.accent : Theme.tertiaryText)
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    private func gameCount(_ folder: LibraryFolderID?) -> String {
        let count = hub.library.games.filter { $0.libraryFolder == folder }.count
        return count == 1 ? "1 game" : "\(count) games"
    }

    /// The SoundFont in use, and the row that opens the file picker for another.
    private var soundFontRow: some View {
        Button { nav.select(.soundFont) } label: {
            VStack(alignment: .leading, spacing: 6) {
                HStack {
                    Label("SoundFont", systemImage: "pianokeys").foregroundStyle(.white)
                    Spacer()
                    Text("Choose…").foregroundStyle(Theme.accent)
                }
                Text(nav.soundFontError ?? nav.soundFontName ?? "Built-in: FluidR3 Mono")
                    .font(.footnote).foregroundStyle(Theme.secondaryText).fixedSize(horizontal: false, vertical: true)
            }
            .contentShape(.rect)
        }
        .buttonStyle(.plain)
        .focusRow(.soundFont, nav)
        .fileImporter(isPresented: Binding(get: { nav.pickingSoundFont }, set: { nav.pickingSoundFont = $0 }),
                      allowedContentTypes: MIDISoundFont.fileExtensions.compactMap { UTType(filenameExtension: $0) }) {
            nav.installSoundFont($0)
        }
    }

    private var savesSummary: String {
        switch hub.saveSync.connection {
        case .notSetUp: hub.saveSync.server == nil ? "Not set up" : "Not tested"
        case .checking: "Connecting…"
        case .connected: hub.saveSync.server?.remote.host ?? "Connected"
        case .failed: "Can't connect"
        }
    }

    /// A font package: its state, and what pressing the row does, which is download, cancel, or remove.
    private func fontRow(_ package: RuntimePackage) -> some View {
        let target = Navigator.Target.fontPackage(package.id)
        let phase = nav.runtimes.phases[package.id]
        let installed = nav.runtimes.installedFonts.contains(package.id)
        let (state, action): (String, String) = switch phase {
        case let .running(progress)?: (RuntimeSection.status(progress), "Cancel")
        case let .failed(message)?: (message, "Download, \(RuntimeSection.size(package.size))")
        case nil: installed ? ("Installed", "Remove") : ("Not installed", "Download, \(RuntimeSection.size(package.size))")
        }
        return Button { nav.select(target) } label: {
            VStack(alignment: .leading, spacing: 6) {
                HStack {
                    Label(package.name, systemImage: "textformat").foregroundStyle(.white)
                    Spacer()
                    Text(action).foregroundStyle(Theme.accent)
                }
                Text(state).font(.footnote).foregroundStyle(Theme.secondaryText).fixedSize(horizontal: false, vertical: true)
                if case let .running(progress)? = phase {
                    ProgressView(value: RuntimeSection.fraction(progress, package)).tint(Theme.accent)
                }
            }
            .contentShape(.rect)
        }
        .buttonStyle(.plain)
        .focusRow(target, nav)
    }

    /// A row that opens a settings page. It is a focus target, so a controller can reach it.
    private func pageRow(_ title: String, systemImage: String, value: String, page: Navigator.SettingsPage) -> some View {
        let target = Navigator.Target.settingsPage(page)
        return Button { nav.select(target) } label: {
            HStack {
                Label(title, systemImage: systemImage).foregroundStyle(.white)
                Spacer()
                Text(value).foregroundStyle(Theme.secondaryText).lineLimit(1)
                Image(systemName: "chevron.right").font(.footnote.weight(.semibold)).foregroundStyle(Theme.tertiaryText)
            }
            .contentShape(.rect)
        }
        .buttonStyle(.plain)
        .focusRow(target, nav)
    }

    static func displayPath(_ url: URL) -> String {
        let path = url.path
        #if os(macOS)
        let home = FileManager.default.homeDirectoryForCurrentUser.path
        return path.hasPrefix(home) ? "~" + path.dropFirst(home.count) : path
        #else
        // The app's own Documents folder is On My iPhone or iPad, VNDegen, and the Files app's local storage is On My
        // iPhone or iPad. A drive or a server connected in Files lives under LiveFiles, in a folder named for its
        // provider and then the volume. Anything else shows its path.
        // Symlinks resolved on both sides, since iOS reaches the app container as /var and as /private/var.
        let full = url.resolvingSymlinksInPath().path
        let device = "On My \(UIDevice.current.localizedModel)"
        let documents = URL.documentsDirectory.resolvingSymlinksInPath().path + "/"
        func trail(_ rest: Substring) -> String { rest.split(separator: "/").joined(separator: " › ") }
        if full.hasPrefix(documents) { return "\(device) › VNDegen › " + trail(full.dropFirst(documents.count)) }
        if let range = full.range(of: "/File Provider Storage/") { return "\(device) › " + trail(full[range.upperBound...]) }
        if let range = full.range(of: "/LiveFiles/") {
            // Drops the provider's folder, such as com.apple.filesystems.userfsd, which names no place the player knows.
            return trail(full[range.upperBound...].drop { $0 != "/" })
        }
        return path
        #endif
    }
}

extension View {
    /// Makes a form row a focus target: pointer hover focuses it, and focus draws the hub's ring around the row.
    func focusRow(_ target: Navigator.Target, _ nav: Navigator) -> some View {
        onHover { if $0 { nav.hover(target) } }
            .focusRow(nav.isFocused(target), id: target)
    }
}

/// A settings page that is a whole form of its own, in a sheet over the settings screen.
struct SettingsPageSheet: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav
    let page: Navigator.SettingsPage

    var body: some View {
        Group {
            switch page {
            case .metadata:
                SheetScaffold(title: "Metadata sources", escapeCloses: true) {
                    MetadataSettingsView(model: nav.metadataSettings).scrollContentBackground(.hidden)
                }
            case .lock:
                SheetScaffold(title: "Hidden games") {
                    LockSettingsView(model: nav.lockSettings).scrollContentBackground(.hidden)
                }
            case .saves:
                SheetScaffold(title: "Ludusavi backups", escapeCloses: true) {
                    SaveSyncSettingsView(model: nav.saveSettings).scrollContentBackground(.hidden)
                }
            case .plugins:
                SheetScaffold(title: "Plugins", subtitle: "Engines built into VNDegen", escapeCloses: true) {
                    PluginsView()
                }
            case .licenses:
                SheetScaffold(title: "Licenses", subtitle: "Third-party code in VNDegen", escapeCloses: true) {
                    LicensesView()
                }
            }
        }
        .presentationDetents([.large])
        #if os(macOS)
        .frame(minWidth: 520, minHeight: 560)
        #endif
    }
}

/// Opening the games folder in the system file browser, and how to add games there.
enum FileBrowser {
    #if os(macOS)
    static let openTitle = "Show in Finder"
    static let instructions = "Copy each game into its own folder inside ~/Games/VNDegen, then rescan. A zip, RAR, or 7z archive copied there shows up in the library under Ready to import."
    static let addedFolders = "Add folder takes another folder of game folders, such as one on an external drive or a server. Its games join the library, and stay listed as unavailable while the drive or server is away. Removing a folder takes its games out of the library and leaves its files alone."
    /// Shows `url` in Finder. `create` makes it first, for VNDegen's own games folder. Other folders are never made, so
    /// a folder on a drive that went away isn't made again on the disk under it.
    static func open(_ url: URL, create: Bool = false) {
        if create { try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true) }
        NSWorkspace.shared.activateFileViewerSelecting([url])
    }
    #else
    static let openTitle = "Open in Files"
    static let addedFolders = "Add folder takes another folder of game folders from Files, such as one on an external drive or a server connected in Files. Its games join the library, and stay listed as unavailable while the drive or server is away. Removing a folder takes its games out of the library and leaves its files alone."
    static let instructions = "In the Files app, open On My \(UIDevice.current.localizedModel), then VNDegen, then Games. Copy each game into its own folder there. A zip, RAR, or 7z archive copied into VNDegen or Games shows up in the library under Ready to import."
    /// Opens `url` in the Files app. `create` makes it first, for VNDegen's own games folder.
    static func open(_ url: URL, create: Bool = false) {
        if create { try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true) }
        // The Files app opens a folder in an app's container through the shareddocuments scheme.
        if let files = URL(string: "shareddocuments://\(url.path)") { UIApplication.shared.open(files) }
    }
    #endif
}

struct EmptyLibraryView: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav
    @Environment(\.hubMetrics) private var metrics

    private var steps: [String] {
        #if os(macOS)
        ["Open the Games folder in Finder.",
         "Copy each game into its own folder, for example Games/No Socks RPG. Or copy in a zip, RAR, or 7z archive.",
         "Come back here. VNDegen adds game folders and lists archives under Ready to import."]
        #else
        let device = UIDevice.current.localizedModel
        return ["Open the Files app and go to On My \(device), then VNDegen.",
                "Copy each game folder into Games, for example Games/No Socks RPG. Or copy a zip, RAR, or 7z archive into VNDegen or Games.",
                "Come back here. VNDegen adds game folders and lists archives under Ready to import."]
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
                    if !hub.localArchives.isEmpty {
                        ReadyToImportList(archives: hub.localArchives)
                    }
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
                    Text("RPG Maker MV and MZ games are supported today. Add game unpacks a zip, RAR, or 7z archive into the library.")
                        .font(.system(size: 14))
                        .foregroundStyle(Theme.tertiaryText)
                    VStack(alignment: .leading, spacing: 12) {
                        HubButton(target: .addGame, kind: .primary) { Label("Add game", systemImage: "plus") }
                        HStack(spacing: 12) {
                            HubButton(target: .openFolder) { Label(FileBrowser.openTitle, systemImage: "folder.fill") }
                            HubButton(target: .rescan) { Label("Rescan", systemImage: "arrow.clockwise") }
                                .disabled(nav.rescanning)
                        }
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
