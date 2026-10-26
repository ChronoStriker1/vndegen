import Foundation
import Observation
import VNCore
import GameInput
import Metadata
import Lock
import SaveSync
import ArchiveImport

/// Where the user is in the hub, what has focus, and how controller events move it.
///
/// SwiftUI focus does not follow gamepads on iOS, so the hub keeps its own focus model. Each screen is a table of rows
/// of `Target`s, derived from the library on demand. Directions move through that table, confirm activates the focused
/// target, and cancel backs out. Touch, mouse, and keyboard drive the same targets through `select` and `hover`.
@MainActor
@Observable
final class Navigator {
    enum Route: Hashable {
        case grid
        case game(GameID)
        case settings
    }

    /// Sheets with state of their own carry its model, so controller input reaches the same state the sheet shows.
    enum Sheet: Identifiable {
        case unlock
        case engine(GameID)
        /// The engine's advanced options for a game, such as the Ren'Py runtime.
        case variant(GameID)
        case orientation(GameID)
        case controlMethod(GameID)
        case mapping(ButtonMappingEditor)
        case artwork(ArtworkEditor)
        case metadata(MetadataSearch)
        case saves(GameSavesModel)
        /// Asked before a game starts when the save server has newer saves.
        case launchSaves(LaunchSavesModel)
        case settingsPage(SettingsPage)
        case importGame(ArchiveImportModel, initialSelection: [URL])
        /// A runtime package's license, which its install waits on.
        case license(LicenseRequest)

        var id: String {
            switch self {
            case .unlock: "unlock"
            case let .engine(id): "engine:\(id)"
            case let .variant(id): "variant:\(id)"
            case let .orientation(id): "orientation:\(id)"
            case let .controlMethod(id): "controlMethod:\(id)"
            case let .mapping(editor): "mapping:\(editor.gameID)"
            case let .artwork(editor): "artwork:\(editor.gameID)"
            case let .metadata(search): "metadata:\(search.gameID)"
            case let .saves(model): "saves:\(model.game)"
            case let .launchSaves(model): "launchSaves:\(model.game)"
            case let .settingsPage(page): "settings:\(page)"
            case .importGame: "import"
            case let .license(request): "license:\(request.id)"
            }
        }
    }

    /// Settings groups that are whole forms of their own, so they open in a sheet instead of inside the settings list.
    enum SettingsPage {
        case metadata, lock, saves, licenses
    }

    enum Shelf: String, CaseIterable {
        case recent, all, hidden

        var title: String {
            switch self {
            case .recent: "Recently played"
            case .all: "All games"
            case .hidden: "Hidden"
            }
        }
    }

    enum Target: Hashable {
        case addGame, gridButton, lockButton, settingsButton
        /// An archive under Ready to import, by its first volume.
        case importArchive(URL)
        case openFolder, rescan
        /// Hero buttons. They act on `heroGame`.
        case play, details
        case tile(Shelf, GameID)
        case gridTile(GameID)
        case back
        case pagePlay, pageArtwork, pageMetadata, pageSaves, pageLock, pageEngine, pageVariant, pageOrientation, pageControls, pageMapping
        case settingsPage(SettingsPage)
        /// Settings pickers, iOS only. Confirm and left and right step through the values.
        case controlMethod, controllerMouse, mouseStyle, playOrientation
        case engineOption(EngineID?)
        case variantOption(String?)
        case orientationOption(PlayOrientation?)
        case controlMethodOption(ControlMethod?)
        case artworkSlot(ArtworkSlot)
        case artworkCandidate(Int)
        case metadataQuery
        case metadataResult(Int)
        /// A runtime package a game is missing, by package ID, on the game's page.
        case runtime(String, RuntimeAction)
        case licenseAccept, licenseDecline
    }

    enum RuntimeAction: Hashable {
        case install, importFolder, cancel
    }

    struct Row {
        let id: String
        let items: [Target]
        /// Shelves remember their column when focus leaves, like Big Picture. Grid rows keep the column instead.
        var remembersColumn = true
    }

    let hub: Hub
    private(set) var path: [Route] = []
    var sheet: Sheet? {
        didSet {
            syncInputMode()
            // Swiping the import sheet away skips its Done button, so finish the import here too.
            if case let .importGame(model, _)? = oldValue, sheet == nil { model.close() }
            // A license sheet closed by Done or a swipe declines it. Accept answers first, so this does nothing then.
            if case let .license(request)? = oldValue { request.respond(false) }
            // Focus moves into a sheet and returns to whatever opened it, however the sheet closes.
            if oldValue == nil, sheet != nil {
                focusBeforeSheet = focus
                setFocus(defaultFocus)
            } else if sheet == nil, oldValue != nil {
                setFocus(focusBeforeSheet)
                focusBeforeSheet = nil
            }
        }
    }
    @ObservationIgnored private var focusBeforeSheet: Target?
    private(set) var focus: Target?
    /// The game shown in the home hero: the last game whose tile had focus.
    private var heroID: GameID?
    /// Set by the grid view from its width.
    var gridColumns = 4
    private(set) var rescanning = false
    @ObservationIgnored private var columnMemory: [String: Int] = [:]
    @ObservationIgnored private var lastBackgrounded: Date?
    /// The unlock view shown in the sheet, kept so controller events reach the same instance.
    @ObservationIgnored private(set) var unlockView: UnlockView?

    /// The game whose saves are being checked before it starts.
    private(set) var checkingSaves: GameID?

    /// The Ludusavi settings page. One for the app, like the service it shows.
    @ObservationIgnored private(set) lazy var saveSettings = SaveSyncSettingsModel(service: hub.saveSync)

    /// Runtime packages games need, such as RPG Maker RTPs, and their installs.
    let runtimes: RuntimeInstalls

    init(hub: Hub) {
        self.hub = hub
        runtimes = RuntimeInstalls(library: hub.library)
        runtimes.onLicense = { [weak self] request in
            guard let self else { return request.respond(false) }
            // Another sheet the player opened meanwhile, such as the artwork editor, gives way to the license.
            sheet = .license(request)
        }
    }

    var route: Route? { path.last }

    // MARK: Library views

    func isVisible(_ game: Game) -> Bool { !game.isLocked || hub.lock.isUnlocked }
    var visibleGames: [Game] { hub.library.games.filter(isVisible) }

    var shelves: [(shelf: Shelf, games: [Game])] {
        let visible = visibleGames
        let all: [(Shelf, [Game])] = [
            (.recent, Array(visible.filter { $0.lastPlayedAt != nil }.prefix(12))),
            (.all, visible.filter { !$0.isLocked }),
            (.hidden, visible.filter(\.isLocked)),
        ]
        return all.filter { !$0.1.isEmpty }.map { (shelf: $0.0, games: $0.1) }
    }

    var heroGame: Game? {
        if let heroID, let game = hub.library.game(heroID), isVisible(game) { return game }
        return shelves.first?.games.first
    }

    // MARK: Focus table

    /// The home screen's Ready to import row, also its scroll anchor.
    static let archivesRowID = "archives"

    func rows() -> [Row] {
        if let sheet {
            switch sheet {
            case .engine, .variant, .orientation, .controlMethod: return choices(for: sheet).map { Row(id: "\($0.target)", items: [$0.target]) }
            case let .artwork(editor):
                let columns = max(1, editor.columns)
                let candidates = stride(from: 0, to: editor.candidates.count, by: columns).enumerated().map { index, start in
                    Row(id: "candidates-\(index)", items: (start..<min(start + columns, editor.candidates.count)).map { .artworkCandidate($0) },
                        remembersColumn: false)
                }
                return [Row(id: "slots", items: ArtworkSlot.allCases.map { .artworkSlot($0) })] + candidates
            case let .metadata(search):
                return [Row(id: "query", items: [.metadataQuery])]
                    + (search.results ?? []).indices.map { Row(id: "result-\($0)", items: [.metadataResult($0)]) }
            case .license: return [Row(id: "license", items: [.licenseAccept, .licenseDecline])]
            // These take controller input through their own models.
            case .unlock, .saves, .launchSaves, .settingsPage, .importGame, .mapping: return []
            }
        }
        switch route {
        case nil:
            let archives = hub.localArchives.map { Target.importArchive($0.url) }
            if visibleGames.isEmpty {
                // The empty library lists archives one under another.
                return [Row(id: "bar", items: [.lockButton, .settingsButton])]
                    + archives.map { Row(id: "\($0)", items: [$0]) }
                    + [Row(id: "empty-add", items: [.addGame]), Row(id: "empty-folder", items: [.openFolder, .rescan])]
            }
            var rows = [Row(id: "bar", items: [.addGame, .gridButton, .lockButton, .settingsButton])]
            if heroGame != nil { rows.append(Row(id: "hero", items: [.play, .details])) }
            if !archives.isEmpty { rows.append(Row(id: Self.archivesRowID, items: archives)) }
            rows += shelves.map { shelf in Row(id: shelf.shelf.rawValue, items: shelf.games.map { .tile(shelf.shelf, $0.id) }) }
            return rows
        case .grid:
            let ids = visibleGames.map(\.id)
            let columns = max(1, gridColumns)
            let tiles = stride(from: 0, to: ids.count, by: columns).enumerated().map { index, start in
                Row(id: "grid-\(index)", items: ids[start..<min(start + columns, ids.count)].map { .gridTile($0) }, remembersColumn: false)
            }
            return [Row(id: "grid-back", items: [.back])] + tiles
        case .game:
            // Orientation only matters on a touch device, and the control method only where it changes something.
            let orientation = Platform.current == .iOS ? [Row(id: "page-orientation", items: [.pageOrientation])] : []
            let controls = pageGameID.map(offersControlMethod) == true ? [Row(id: "page-controls", items: [.pageControls])] : []
            let runtimes = (pageGameID.flatMap { self.runtimes.missing[$0] } ?? []).map { package in
                let running = if case .running? = self.runtimes.phases[package.id] { true } else { false }
                let items: [Target] = running ? [.runtime(package.id, .cancel)]
                    : [.runtime(package.id, .install), .runtime(package.id, .importFolder)]
                return Row(id: "page-runtime-\(package.id)", items: items)
            }
            return [
                Row(id: "page-back", items: [.back]),
                Row(id: "page-actions", items: [.pagePlay, .pageArtwork, .pageMetadata, .pageSaves, .pageLock]),
            ] + runtimes + [
                Row(id: "page-engine", items: [.pageEngine]),
            ] + (pageGameID.flatMap(variants) == nil ? [] : [Row(id: "page-variant", items: [.pageVariant])])
                + orientation + controls + [Row(id: "page-mapping", items: [.pageMapping])]
        case .settings:
            var items: [Target] = [.back, .openFolder, .rescan, .settingsPage(.metadata), .settingsPage(.lock), .settingsPage(.saves)]
            #if os(iOS)
            let method = ControlMethod.setting
            items.append(.controlMethod)
            if method.kind == .controller { items.append(.controllerMouse) }
            if method.mouse != nil { items.append(.mouseStyle) }
            items.append(.playOrientation)
            #endif
            // In screen order, so Licenses, at the bottom, comes last.
            items.append(.settingsPage(.licenses))
            return items.map { Row(id: "settings-\($0)", items: [$0]) }
        }
    }

    private var defaultFocus: Target? {
        if let sheet {
            switch sheet {
            case let .artwork(editor): return .artworkSlot(editor.slot)
            case .metadata: return .metadataQuery
            default: return selectedChoice(for: sheet)
            }
        }
        switch route {
        case nil:
            if visibleGames.isEmpty { return hub.localArchives.first.map { .importArchive($0.url) } ?? .addGame }
            if let game = heroGame, let shelf = shelves.first(where: { $0.games.contains { $0.id == game.id } }) {
                return .tile(shelf.shelf, game.id)
            }
            return rows().first?.items.first
        case .grid: return visibleGames.first.map { .gridTile($0.id) } ?? .back
        case .game: return .pagePlay
        case .settings: return .back
        }
    }

    enum Direction { case up, down, left, right }

    func move(_ direction: Direction) {
        let rows = rows()
        guard let focus, let (r, c) = position(of: focus, in: rows) else {
            setFocus(defaultFocus)
            return
        }
        switch direction {
        case .left: setFocus(rows[r].items[max(0, c - 1)])
        case .right: setFocus(rows[r].items[min(rows[r].items.count - 1, c + 1)])
        case .up, .down:
            let next = direction == .up ? r - 1 : r + 1
            guard rows.indices.contains(next) else { return }
            enterRow(rows[next], leaving: rows[r], column: c)
        }
    }

    /// Jumps to the start of the previous or next row, for shoulder buttons.
    func page(_ forward: Bool) {
        let rows = rows()
        guard let focus, let (r, _) = position(of: focus, in: rows) else { return setFocus(defaultFocus) }
        let next = forward ? r + 1 : r - 1
        guard rows.indices.contains(next) else { return }
        setFocus(rows[next].items.first)
    }

    private func enterRow(_ row: Row, leaving current: Row, column: Int) {
        if current.remembersColumn { columnMemory[current.id] = column }
        let remembered = row.remembersColumn ? columnMemory[row.id] : nil
        setFocus(row.items[min(remembered ?? column, row.items.count - 1)])
    }

    private func position(of target: Target, in rows: [Row]) -> (Int, Int)? {
        for (r, row) in rows.enumerated() {
            if let c = row.items.firstIndex(of: target) { return (r, c) }
        }
        return nil
    }

    func setFocus(_ target: Target?) {
        focus = target
        switch target {
        case let .tile(_, id), let .gridTile(id): heroID = id
        default: break
        }
    }

    /// Touch users never see a focus ring. Controllers, the keyboard, and the pointer reveal it.
    func isFocused(_ target: Target) -> Bool { focus == target && hub.input.lastInputKind != .touch }

    /// Screens scroll to keep focus in view only when a controller or the keyboard moved it. Scrolling under a hovering
    /// pointer or a touch user's finger would fight them.
    private(set) var focusMovedByController = false
    /// Set once a controller, the keyboard, or the pointer has moved focus. Until then focus follows the library as it loads.
    @ObservationIgnored private var steered = false

    /// Pointer hover on Mac and iPad moves focus without activating.
    func hover(_ target: Target) {
        // Views behind an open sheet are not in the focus table, so they cannot steal focus from it.
        guard position(of: target, in: rows()) != nil else { return }
        hub.input.noteInput(.keyboardMouse)
        focusMovedByController = false
        steered = true
        if focus != target { setFocus(target) }
    }

    /// Tap or click: focus and activate in one step.
    func select(_ target: Target) {
        #if os(macOS)
        hub.input.noteInput(.keyboardMouse)
        #else
        hub.input.noteInput(.touch)
        #endif
        focusMovedByController = false
        setFocus(target)
        activate(target)
    }

    // MARK: Actions

    func activate(_ target: Target) {
        switch target {
        case .addGame: openImport()
        case let .importArchive(url): openImport([url])
        case .openFolder: LibraryFolder.open(hub.library.paths.gamesRoot)
        case .rescan: rescan()
        case .controlMethod, .controllerMouse, .mouseStyle, .playOrientation: stepSetting(target, by: 1)
        case .gridButton: push(.grid)
        case .settingsButton: push(.settings)
        case .lockButton: toggleLibraryLock()
        case .play: if let game = heroGame { play(game.id) }
        case .details: if let game = heroGame { push(.game(game.id)) }
        case let .tile(_, id), let .gridTile(id): push(.game(id))
        case .back: back()
        case .pagePlay: if let id = pageGameID { play(id) }
        case .pageArtwork: if let id = pageGameID { sheet = .artwork(ArtworkEditor(hub: hub, gameID: id)) }
        case .pageMetadata: if let id = pageGameID { sheet = .metadata(MetadataSearch(hub: hub, gameID: id)) }
        case .pageSaves: if let id = pageGameID { sheet = .saves(GameSavesModel(service: hub.saveSync, game: id)) }
        case let .settingsPage(page): sheet = .settingsPage(page)
        case .pageEngine: if let id = pageGameID { sheet = .engine(id) }
        case .pageVariant: if let id = pageGameID { sheet = .variant(id) }
        case .pageOrientation: if let id = pageGameID { sheet = .orientation(id) }
        case .pageControls: if let id = pageGameID, offersControlMethod(id) { sheet = .controlMethod(id) }
        case .pageMapping: if let id = pageGameID { sheet = .mapping(ButtonMappingEditor(library: hub.library, gameID: id)) }
        case .pageLock: if let id = pageGameID { toggleGameLock(id) }
        case let .engineOption(engine):
            if case let .engine(id) = sheet { hub.library.update(id) { $0.engineOverride = engine } }
            sheet = nil
        case let .variantOption(variant):
            if case let .variant(id) = sheet { selectVariant(variant, for: id) }
            sheet = nil
        case let .orientationOption(orientation):
            if case let .orientation(id) = sheet { hub.library.update(id) { $0.orientation = orientation } }
            sheet = nil
        case let .controlMethodOption(method):
            if case let .controlMethod(id) = sheet { hub.library.update(id) { $0.controlMethod = method } }
            sheet = nil
        case let .artworkSlot(slot):
            if case let .artwork(editor) = sheet { editor.slot = slot }
        case let .artworkCandidate(index):
            if case let .artwork(editor) = sheet, editor.candidates.indices.contains(index) { editor.apply(editor.candidates[index]) }
        case .metadataQuery:
            if case let .metadata(search) = sheet { search.editingQuery = true }
        case let .metadataResult(index):
            if case let .metadata(search) = sheet, let results = search.results, results.indices.contains(index) {
                apply(results[index], in: search)
            }
        case let .runtime(packageID, action):
            guard let id = pageGameID, let package = runtimes.missing[id]?.first(where: { $0.id == packageID }) else { return }
            switch action {
            case .install: runtimes.install(package, for: id)
            case .importFolder: runtimes.folderImport = package
            case .cancel: runtimes.cancel(package)
            }
        case .licenseAccept, .licenseDecline:
            guard case let .license(request) = sheet else { return }
            request.respond(target == .licenseAccept)
            sheet = nil
        }
    }

    /// Applies a metadata match and closes its sheet, unless the user already closed it.
    func apply(_ candidate: MetadataCandidate, in search: MetadataSearch) {
        Task {
            guard await search.apply(candidate), case let .metadata(shown)? = sheet, shown === search else { return }
            sheet = nil
        }
    }

    // MARK: Adding games

    /// Opens the import sheet with `urls` chosen. An import sheet already showing takes them instead. Behind another
    /// sheet they are discarded.
    func openImport(_ urls: [URL] = []) {
        if case let .importGame(model, _)? = sheet, !urls.isEmpty { return model.choose(urls) }
        guard sheet == nil else { return ArchiveImporter.discard(urls) }
        sheet = .importGame(ArchiveImportModel(gamesRoot: hub.library.paths.gamesRoot), initialSelection: urls)
    }

    /// Deletes Inbox copies nothing imports. Call each time the app goes to the background. A copy is deleted once it
    /// sat through a whole foreground session with no import reading it, so the first background of a run deletes none.
    func discardInboxCopies() {
        if let lastBackgrounded { ArchiveImporter.discardInboxCopies(addedBefore: lastBackgrounded) }
        lastBackgrounded = .now
    }

    /// Closes the import sheet, then shows the new game in the library.
    func finishImport(_ folderName: String?) {
        if case .importGame? = sheet { sheet = nil }
        guard let folderName else { return }
        Task {
            await hub.refreshLibrary()
            let id = GameID(folderName)
            guard hub.library.game(id) != nil else { return }
            heroID = id
            if let tile = rows().lazy.flatMap(\.items).first(where: { $0.gameID == id }) { setFocus(tile) }
        }
    }

    private func rescan() {
        guard !rescanning else { return }
        rescanning = true
        Task {
            await hub.refreshLibrary()
            rescanning = false
        }
    }

    /// Steps a settings picker through its values. Returns false for targets that are not pickers.
    @discardableResult
    private func stepSetting(_ target: Target, by step: Int) -> Bool {
        #if os(iOS)
        switch target {
        case .controlMethod: setControlMethod(ControlMethod.setting.stepped(by: step))
        case .controllerMouse: setControlMethod(ControlMethod.setting.togglingMouse)
        case .mouseStyle: setControlMethod(ControlMethod.setting.switchingMouseStyle)
        case .playOrientation: cycle(PlayOrientation.storageKey, fallback: PlayOrientation.setting, by: step)
        default: return false
        }
        return true
        #else
        return false
        #endif
    }

    #if os(iOS)
    /// Writes to the defaults key that the settings screen's `@AppStorage` reads. A mouse row the new method hides hands
    /// focus to the control method row.
    func setControlMethod(_ method: ControlMethod) {
        UserDefaults.standard.set(method.rawValue, forKey: ControlMethod.storageKey)
        if let focus, focus == .controllerMouse || focus == .mouseStyle, position(of: focus, in: rows()) == nil {
            setFocus(.controlMethod)
        }
    }

    /// Writes the next value to the defaults key that the settings screen's `@AppStorage` reads.
    private func cycle<Value: CaseIterable & RawRepresentable<String> & Equatable>(_ key: String, fallback: Value, by step: Int) {
        let values = Array(Value.allCases)
        let current = UserDefaults.standard.string(forKey: key).flatMap(Value.init(rawValue:)) ?? fallback
        let index = values.firstIndex(of: current) ?? 0
        UserDefaults.standard.set(values[(index + step + values.count) % values.count].rawValue, forKey: key)
    }
    #endif

    /// Bumped when an engine option changes, so views that show one read it again. The engine keeps it outside the
    /// library.
    private(set) var variantRevision = 0

    /// The game's engine options, such as the Ren'Py runtime on macOS. Nil when its engine has none here.
    func variants(_ id: GameID) -> EngineVariants? {
        _ = variantRevision
        guard let game = hub.library.game(id), let plugin = hub.library.plugin(for: game.engine) else { return nil }
        return plugin.variants(for: game, dataDirectory: hub.library.paths.dataDirectory(for: id))
    }

    private func selectVariant(_ variant: String?, for id: GameID) {
        guard let game = hub.library.game(id), let plugin = hub.library.plugin(for: game.engine) else { return }
        do {
            try plugin.selectVariant(variant, for: game, dataDirectory: hub.library.paths.dataDirectory(for: id))
        } catch {
            hub.launchError = error.localizedDescription
        }
        variantRevision += 1
    }

    /// Whether the game's page offers a control method: not on a Mac, and not for a game that reads touches itself,
    /// such as a Ren'Py game.
    func offersControlMethod(_ id: GameID) -> Bool {
        ControlMethod.applies(to: hub.library.plugin(for: hub.library.game(id)?.engine))
    }

    var pageGameID: GameID? {
        if case let .game(id) = route { return id }
        return nil
    }

    func push(_ route: Route) {
        path.append(route)
        syncInputMode()
        setFocus(defaultFocus)
    }

    func back() {
        if sheet != nil {
            sheet = nil
            return
        }
        guard let leaving = path.popLast() else { return }
        syncInputMode()
        // Return focus to the tile that opened the page, so the hero and shelf position survive the round trip.
        if case let .game(id) = leaving, let tile = rows().lazy.flatMap(\.items).first(where: { $0.gameID == id }) {
            setFocus(tile)
        } else {
            setFocus(leaving == .grid ? .gridButton : leaving == .settings ? .settingsButton : defaultFocus)
        }
    }

    func toggleLibraryLock() {
        if hub.lock.isUnlocked {
            relock()
        } else {
            presentUnlock()
        }
    }

    /// Shows the unlock sheet, which asks for a new passcode first when nothing could unlock the library yet.
    private func presentUnlock(then done: @escaping (Bool) -> Void = { _ in }) {
        unlockView = UnlockView(lock: hub.lock) { [weak self] unlocked in
            guard let self else { return }
            if case .unlock = self.sheet { self.sheet = nil }
            self.unlockView = nil
            self.revalidate()
            done(unlocked)
        }
        sheet = .unlock
    }

    /// Locks the library and drops any screen that shows a locked game.
    func relock() {
        hub.lock.lock()
        revalidate()
    }

    private func toggleGameLock(_ id: GameID) {
        guard let game = hub.library.game(id) else { return }
        hub.lock.refreshBiometry()
        if !game.isLocked, hub.lock.needsPasscodeSetup {
            // A game hidden with nothing to unlock it would not be protected, so the user sets a passcode first.
            presentUnlock { [weak self] _ in
                guard let self, !self.hub.lock.needsPasscodeSetup else { return }
                self.setLocked(true, id)
            }
            return
        }
        setLocked(!game.isLocked, id)
    }

    private func setLocked(_ locked: Bool, _ id: GameID) {
        hub.library.update(id) { $0.isLocked = locked }
        revalidate()
    }

    /// Called after the library or lock state changes, so no screen or focus points at a game that is now hidden or gone.
    func revalidate() {
        let shown = Set(visibleGames.map(\.id))
        let trimmed = path.prefix { route in
            if case let .game(id) = route { return shown.contains(id) }
            return true
        }
        if trimmed.count != path.count {
            path = Array(trimmed)
            sheet = nil
            syncInputMode()
        }
        // A rescan can find that an open control method sheet's game reads touches itself, such as a Ren'Py game whose
        // files finished copying. Closing it returns focus to its row, which the focus check below then moves.
        if case let .controlMethod(id)? = sheet, !offersControlMethod(id) { sheet = nil }
        // Until the user steers, and always for touch users, keep the default focus current as the library loads.
        if !steered || hub.input.lastInputKind == .touch || focus.map({ position(of: $0, in: rows()) == nil }) ?? true { setFocus(defaultFocus) }
    }

    // MARK: Choice sheets

    /// One option in a sheet that picks a single value, such as the engine override.
    struct Choice: Identifiable {
        let target: Target
        let title: String
        var detail: String?
        var id: Target { target }
    }

    func choices(for sheet: Sheet) -> [Choice] {
        switch sheet {
        case let .engine(id):
            // "Automatic (detected: X)" first, then every engine that runs on this platform.
            let detected = hub.library.game(id)?.detected.flatMap { hub.library.plugin(for: $0.engine)?.descriptor.displayName }
            let manual = hub.library.engines
                .map(\.descriptor)
                .filter { $0.platforms.contains(.current) }
                .map { Choice(target: .engineOption($0.id), title: $0.displayName) }
            return [Choice(target: .engineOption(nil), title: "Automatic", detail: "Detected: \(detected ?? "none")")] + manual
        case let .variant(id):
            guard let variants = variants(id) else { return [] }
            return [Choice(target: .variantOption(nil), title: "Automatic", detail: variants.automatic)]
                + variants.options.map { Choice(target: .variantOption($0.id), title: $0.title) }
        case .orientation:
            let fixed = PlayOrientation.allCases.map { Choice(target: .orientationOption($0), title: $0.title) }
            return [Choice(target: .orientationOption(nil), title: "Use default", detail: "Currently \(PlayOrientation.setting.title), from Settings")] + fixed
        case .controlMethod:
            let fixed = ControlMethod.allCases.map { Choice(target: .controlMethodOption($0), title: $0.title, detail: $0.detail) }
            return [Choice(target: .controlMethodOption(nil), title: "Use default", detail: "Currently \(ControlMethod.setting.title), from Settings")] + fixed
        case .unlock, .artwork, .metadata, .saves, .launchSaves, .settingsPage, .importGame, .mapping, .license:
            return []
        }
    }

    func selectedChoice(for sheet: Sheet) -> Target? {
        switch sheet {
        case let .engine(id): .engineOption(hub.library.game(id)?.engineOverride)
        case let .variant(id): .variantOption(variants(id)?.selected)
        case let .orientation(id): .orientationOption(hub.library.game(id)?.orientation)
        case let .controlMethod(id): .controlMethodOption(hub.library.game(id)?.controlMethod)
        case .unlock, .artwork, .metadata, .saves, .launchSaves, .settingsPage, .importGame, .mapping, .license: nil
        }
    }

    /// Starts a game, first asking about newer saves on the save server when there are any.
    func play(_ id: GameID) {
        guard checkingSaves == nil else { return }
        checkingSaves = id
        Task { [weak self] in
            guard let self else { return }
            let offer = await hub.saveSync.launchOffer(id)
            checkingSaves = nil
            guard let offer else { return hub.launch(id) }
            sheet = .launchSaves(LaunchSavesModel(service: hub.saveSync, game: id, offer: offer) { [weak self] start in
                self?.sheet = nil
                if start { self?.hub.launch(id) }
            })
        }
    }

    // MARK: Input

    /// Takes over controller input for the hub. Called whenever the library is on screen.
    func attachInput() {
        hub.input.onEvent = { [weak self] event in self?.handle(event) }
        hub.input.repeatsDirections = true
        syncInputMode()
    }

    /// The settings screen and its pages, the metadata search, the Ludusavi game search in the saves sheet, and the import
    /// sheet's folder name and password have text fields that need raw keyboard input.
    private func syncInputMode() {
        guard case .library = hub.screen else { return }
        let typing: Bool
        switch (sheet, route) {
        case (.metadata?, _), (.saves?, _), (.settingsPage?, _), (.importGame?, _), (nil, .settings?): typing = true
        default: typing = false
        }
        hub.input.translatesKeyboard = !typing
    }

    func handle(_ event: ControllerEvent) {
        focusMovedByController = true
        steered = true
        if case .unlock = sheet {
            unlockView?.handle(event)
            return
        }
        guard case let .button(button, pressed: true) = event else { return }
        switch sheet {
        case let .saves(model)?:
            if !model.handle(button), button == .cancel { back() }
            return
        case let .launchSaves(model)?:
            _ = model.handle(button)
            return
        case .settingsPage(.saves)?:
            if !saveSettings.handle(button), button == .cancel { back() }
            return
        case let .mapping(editor)?:
            if !editor.handle(button), button == .cancel { back() }
            return
        case let .importGame(model, _)?:
            if let button = ArchiveImportModel.Button(button) { model.handle(button) }
            return
        case let .artwork(editor)?:
            switch button {
            // Shoulder buttons switch slots from anywhere in the sheet.
            case .pageUp, .pageDown:
                editor.cycleSlot(button == .pageUp ? -1 : 1)
                setFocus(.artworkSlot(editor.slot))
                return
            case .left, .right:
                move(button == .left ? .left : .right)
                // The slot row works like tabs: moving along it switches the slot.
                if case let .artworkSlot(slot)? = focus { editor.slot = slot }
                return
            default: break
            }
        default: break
        }
        switch button {
        case .up: move(.up)
        case .down: move(.down)
        case .left, .right:
            if let focus, stepSetting(focus, by: button == .left ? -1 : 1) { break }
            move(button == .left ? .left : .right)
        case .confirm: if let focus { activate(focus) } else { setFocus(defaultFocus) }
        case .cancel: back()
        case .menu: if sheet == nil, route != .settings { push(.settings) }
        case .pageUp: page(false)
        case .pageDown: page(true)
        case .dash, .hub: break
        }
    }
}

extension ArchiveImportModel.Button {
    init?(_ button: VirtualButton) {
        switch button {
        case .up: self = .up
        case .down: self = .down
        case .left: self = .left
        case .right: self = .right
        case .confirm: self = .confirm
        case .cancel: self = .cancel
        default: return nil
        }
    }
}

extension Navigator.Target {
    var gameID: GameID? {
        switch self {
        case let .tile(_, id), let .gridTile(id): id
        default: nil
        }
    }
}
