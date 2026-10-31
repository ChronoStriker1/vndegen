import FontPack
import Foundation
import Observation
import VNCore
import GameInput
import Metadata
import Lock
import SaveSync
import ArchiveImport
import ControllerUI
#if canImport(UIKit)
import UIKit
#endif

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
        /// A question asked before a change, such as removing a library folder.
        case confirm(Confirmation)
        /// A library tile's menu, which a controller opens. Touch and the pointer get the same actions as a context menu.
        case gameMenu(GameID)
        /// A game page's screenshots full screen. It covers the hub as an overlay rather than a sheet, so it can fill the
        /// screen on every platform.
        case screenshots(ScreenshotViewer)

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
            case let .confirm(confirmation): "confirm:\(confirmation.id)"
            case let .gameMenu(id): "gameMenu:\(id)"
            case let .screenshots(viewer): "screenshots:\(viewer.gameID)"
            }
        }

        /// The game the sheet is about, if any.
        var gameID: GameID? {
            switch self {
            case let .engine(id), let .variant(id), let .orientation(id), let .controlMethod(id), let .gameMenu(id): id
            case let .mapping(editor): editor.gameID
            case let .artwork(editor): editor.gameID
            case let .metadata(search): search.gameID
            case let .saves(model): model.game
            case let .launchSaves(model): model.game
            case let .confirm(confirmation): confirmation.gameID
            case let .screenshots(viewer): viewer.gameID
            case .unlock, .settingsPage, .importGame, .license: nil
            }
        }
    }

    /// Settings groups that are whole forms of their own, so they open in a sheet instead of inside the settings list.
    enum SettingsPage {
        case metadata, lock, saves, plugins, licenses
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
        case addGame, layoutButton, gridButton, lockButton, settingsButton
        /// An archive under Ready to import, by its first volume.
        case importArchive(URL)
        case openFolder, rescan
        /// Hero buttons. They act on `heroGame`.
        case play, details
        case tile(Shelf, GameID)
        case gridTile(GameID)
        case back
        case pagePlay, pageArtwork, pageMetadata, pageSaves, pageLock, pageShowFolder, pageRemove, pageEngine, pageVariant, pageOrientation, pageControls, pageMapping
        /// A screenshot in the game page's grid, by its index in the game's screenshots, and the button under the grid
        /// that shows all of them or fewer.
        case pageScreenshot(Int), pageScreenshotsMore
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
        /// A font package in Settings, by package ID. It downloads, cancels, or removes the package.
        case fontPackage(String)
        /// The MIDI SoundFont rows in Settings: choose a file, and go back to the bundled font.
        case soundFont, soundFontReset
        /// Settings' library folder rows: add one, and show or remove an added one.
        case addLibraryFolder, showLibraryFolder(LibraryFolderID), removeLibraryFolder(LibraryFolderID)
        /// The confirmation sheet's buttons.
        case confirmAction, confirmCancel
        /// An action in a library tile's menu.
        case gameAction(GameAction)
    }

    /// What a library tile's menu offers, which the game page offers too.
    enum GameAction: Hashable {
        case play, openPage, artwork, metadata, hide, showFolder, remove
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
            // The viewer hands focus to the screenshot it showed last, so the player carries on from there in the grid.
            // A metadata match applied meanwhile can change the page's screenshots, so it is found again by URL.
            if case let .screenshots(viewer)? = oldValue, sheet == nil, pageGameID == viewer.gameID {
                let current = pageScreenshots, url = viewer.screenshots[viewer.index].url
                focusBeforeSheet = current.firstIndex { $0.url == url }.map { .pageScreenshot($0) }
                    ?? (current.isEmpty ? .pagePlay : .pageScreenshot(0))
            }
            // Focus moves into a sheet and returns to whatever opened it, however the sheet closes. A sheet that replaces
            // another, such as a license over the artwork editor, starts on its own default.
            if oldValue == nil, sheet != nil {
                focusBeforeSheet = focus
                setFocus(defaultFocus)
            } else if sheet == nil, oldValue != nil {
                // The window can resize under a sheet, which changes the screenshot grid it returns to.
                setFocus(shownScreenshotTarget(focusBeforeSheet))
                focusBeforeSheet = nil
            } else if let sheet, let oldValue, sheet.id != oldValue.id {
                setFocus(defaultFocus)
            }
            if sheet?.id != oldValue?.id {
                pager = ScrollPager()
                // A swipe can close the Ludusavi page under its keyboard, which would otherwise be open next time.
                if case .settingsPage(.saves)? = oldValue { saveSettings.keyboard?.close() }
            }
        }
    }
    /// The sheet SwiftUI presents: every sheet but the screenshot viewer, which fills the screen, something a sheet can't
    /// do on a Mac or an iPad, so the library shows it as an overlay.
    var presentedSheet: Sheet? {
        get { if case .screenshots? = sheet { nil } else { sheet } }
        set { sheet = newValue }
    }
    @ObservationIgnored private var focusBeforeSheet: Target?
    private(set) var focus: Target?
    /// The game shown in the home hero: the last game whose tile had focus.
    private var heroID: GameID?
    /// Set by the grid view from its width.
    var gridColumns = 4
    var layout = LibraryLayout.setting {
        didSet { UserDefaults.standard.set(layout.rawValue, forKey: LibraryLayout.storageKey) }
    }
    /// Set by the home screen's header list from its width.
    var headerColumns = 1
    /// Set by the game page when a narrow window puts its other buttons in a row under Play.
    var pageActionsStacked = false
    /// Set by the game page's screenshot grid from its width.
    var screenshotColumns = 2 {
        didSet { if screenshotColumns != oldValue, sheet == nil { setFocus(shownScreenshotTarget(focus)) } }
    }
    /// Whether the game page's screenshot grid shows every screenshot rather than its first rows. Each page starts collapsed.
    var showsAllScreenshots = false
    private(set) var rescanning = false
    /// Settings' SoundFont file picker is open.
    var pickingSoundFont = false
    /// Settings' folder picker for a new library folder is open.
    var pickingLibraryFolder = false
    /// The title of the alert showing `hub.launchError`, when it reports something other than a failed launch.
    var errorTitle: String?
    /// The open import sheet's model, which knows the library folder the game went into after the sheet closes.
    @ObservationIgnored private var importing: ArchiveImportModel?
    /// The file name of the player's MIDI SoundFont, nil for the bundled one, and why the last choice failed.
    private(set) var soundFontName = MIDISoundFont.userFont()?.lastPathComponent
    private(set) var soundFontError: String?
    @ObservationIgnored private var columnMemory: [String: Int] = [:]
    @ObservationIgnored private var lastBackgrounded: Date?
    /// The unlock view shown in the sheet, kept so controller events reach the same instance.
    @ObservationIgnored private(set) var unlockView: UnlockView?

    /// The game whose saves are being checked before it starts.
    private(set) var checkingSaves: GameID?

    /// The Ludusavi settings page. One for the app, like the service it shows.
    @ObservationIgnored private(set) lazy var saveSettings = SaveSyncSettingsModel(service: hub.saveSync)
    /// The Metadata sources and Hidden games pages, made again each time their page opens.
    @ObservationIgnored private(set) lazy var metadataSettings = MetadataSettingsModel(service: hub.metadata)
    @ObservationIgnored private(set) lazy var lockSettings = LockSettingsModel(lock: hub.lock)
    /// Scrolls the open sheet's text, such as a license, which has nothing to focus. Each sheet gets a new one.
    private(set) var pager = ScrollPager()

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
        // The header list stacks every game, so Recently played would only repeat the top of All games, which already
        // lists the last played first.
        let recent = layout == .headers ? [] : Array(visible.filter { $0.lastPlayedAt != nil }.prefix(12))
        let all: [(Shelf, [Game])] = [
            (.recent, recent),
            (.all, visible.filter { !$0.isLocked }),
            (.hidden, visible.filter(\.isLocked)),
        ]
        return all.filter { !$0.1.isEmpty }.map { (shelf: $0.0, games: $0.1) }
    }

    var heroGame: Game? {
        // Only a game still in the library, since the library keeps the records of games whose folders went away.
        if let heroID, let game = visibleGames.first(where: { $0.id == heroID }) { return game }
        return shelves.first?.games.first
    }

    // MARK: Focus table

    /// The home screen's Ready to import row, also its scroll anchor.
    static let archivesRowID = "archives"

    func rows() -> [Row] {
        if let sheet {
            switch sheet {
            case .engine, .variant, .orientation, .controlMethod, .gameMenu: return choices(for: sheet).map { Row(id: "\($0.target)", items: [$0.target]) }
            case let .artwork(editor):
                let candidates = Self.gridRows("candidates", editor.candidates.indices.map { .artworkCandidate($0) }, columns: editor.columns)
                return [Row(id: "slots", items: ArtworkSlot.allCases.map { .artworkSlot($0) })] + candidates
            case let .metadata(search):
                return [Row(id: "query", items: [.metadataQuery])]
                    + (search.results ?? []).indices.map { Row(id: "result-\($0)", items: [.metadataResult($0)]) }
            case .license: return [Row(id: "license", items: [.licenseAccept, .licenseDecline])]
            case .confirm: return [Row(id: "confirm", items: [.confirmAction, .confirmCancel])]
            // These take controller input through their own models.
            case .unlock, .saves, .launchSaves, .settingsPage, .importGame, .mapping, .screenshots: return []
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
            var rows = [Row(id: "bar", items: [.addGame, .layoutButton, .gridButton, .lockButton, .settingsButton])]
            if layout == .featured, heroGame != nil { rows.append(Row(id: "hero", items: [.play, .details])) }
            if !archives.isEmpty { rows.append(Row(id: Self.archivesRowID, items: archives)) }
            for (shelf, games) in shelves {
                let tiles = games.map { Target.tile(shelf, $0.id) }
                if layout == .headers {
                    rows += Self.gridRows(shelf.rawValue, tiles, columns: headerColumns)
                } else {
                    rows.append(Row(id: shelf.rawValue, items: tiles))
                }
            }
            return rows
        case .grid:
            return [Row(id: "grid-back", items: [.back])] + Self.gridRows("grid", visibleGames.map { .gridTile($0.id) }, columns: gridColumns)
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
            // The screenshots sit under the actions in a narrow window, and in the column beside the details in a wide one.
            // Either way they come before the engine row, so a controller reaches them straight from Play.
            let screenshots = pageScreenshots
            let columns = max(1, screenshotColumns)
            let shown = Self.shownScreenshots(screenshots.count, columns: columns, showsAll: showsAllScreenshots)
            let screenshotRows = Self.gridRows("page-screenshots", (0..<shown).map { Target.pageScreenshot($0) }, columns: columns)
                + (Self.collapsesScreenshots(screenshots.count, columns: columns) ? [Row(id: "page-screenshots-more", items: [.pageScreenshotsMore])] : [])
            let secondary: [Target] = [.pageArtwork, .pageMetadata, .pageSaves, .pageLock, .pageShowFolder, .pageRemove]
            let pageActions = pageActionsStacked
                ? [Row(id: "page-play", items: [.pagePlay]), Row(id: "page-actions", items: secondary)]
                : [Row(id: "page-actions", items: [.pagePlay] + secondary)]
            return [
                Row(id: "page-back", items: [.back]),
            ] + pageActions + runtimes + screenshotRows + [
                Row(id: "page-engine", items: [.pageEngine]),
            ] + (pageGameID.flatMap(variants) == nil ? [] : [Row(id: "page-variant", items: [.pageVariant])])
                + orientation + controls + [Row(id: "page-mapping", items: [.pageMapping])]
        case .settings:
            var rows = [Row(id: "settings-back", items: [.back]), Row(id: "settings-openFolder", items: [.openFolder])]
            rows += hub.library.folders.map { Row(id: "settings-folder-\($0.id)", items: [.showLibraryFolder($0.id), .removeLibraryFolder($0.id)]) }
            var items: [Target] = [.addLibraryFolder, .rescan, .settingsPage(.metadata), .settingsPage(.lock), .settingsPage(.saves)]
            #if os(iOS)
            let method = ControlMethod.setting
            items.append(.controlMethod)
            if method.kind == .controller { items.append(.controllerMouse) }
            if method.mouse != nil { items.append(.mouseStyle) }
            items.append(.playOrientation)
            #endif
            items.append(.settingsPage(.plugins))
            items += FontPack.packages.map { .fontPackage($0.id) }
            items.append(.soundFont)
            if soundFontName != nil { items.append(.soundFontReset) }
            // In screen order, so Licenses, at the bottom, comes last.
            items.append(.settingsPage(.licenses))
            return rows + items.map { Row(id: "settings-\($0)", items: [$0]) }
        }
    }

    /// The open game page's screenshots, empty until a lookup finds some.
    var pageScreenshots: [Screenshot] {
        pageGameID.flatMap { hub.library.game($0)?.metadata?.screenshots } ?? []
    }

    /// How many of `total` screenshots the game page's grid shows at `columns`: the first two rows until the player asks
    /// for all, unless three rows hold them all, since hiding a few behind a button saves little.
    static func shownScreenshots(_ total: Int, columns: Int, showsAll: Bool) -> Int {
        showsAll || !collapsesScreenshots(total, columns: columns) ? total : columns * 2
    }

    /// Whether the grid has more screenshots than it shows collapsed, so it offers Show all and Show fewer.
    static func collapsesScreenshots(_ total: Int, columns: Int) -> Bool { total > columns * 3 }

    /// Where focus on `target` belongs after the screenshot grid changed under it, such as when the window narrowed or
    /// widened. A screenshot the grid no longer shows makes it show all of them, and a Show all button that went away,
    /// because the grid no longer collapses, hands focus to the last screenshot.
    private func shownScreenshotTarget(_ target: Target?) -> Target? {
        let total = pageScreenshots.count
        switch target {
        case let .pageScreenshot(index)? where index >= total:
            return total > 0 ? .pageScreenshot(total - 1) : .pagePlay
        case let .pageScreenshot(index)? where index >= Self.shownScreenshots(total, columns: screenshotColumns, showsAll: showsAllScreenshots):
            showsAllScreenshots = true
            return target
        case .pageScreenshotsMore? where !Self.collapsesScreenshots(total, columns: screenshotColumns):
            return total > 0 ? .pageScreenshot(total - 1) : .pagePlay
        default:
            return target
        }
    }

    /// `items` in rows of `columns`. Moving between them keeps the column, as moving through a grid should.
    private static func gridRows(_ id: String, _ items: [Target], columns: Int) -> [Row] {
        let columns = max(1, columns)
        return stride(from: 0, to: items.count, by: columns).enumerated().map { index, start in
            Row(id: "\(id)-\(index)", items: Array(items[start..<min(start + columns, items.count)]), remembersColumn: false)
        }
    }

    private var defaultFocus: Target? {
        if let sheet {
            switch sheet {
            case let .artwork(editor): return .artworkSlot(editor.slot)
            case .metadata: return .metadataQuery
            // The player asked to install the package, so Accept is the likely answer.
            case .license: return .licenseAccept
            // A slip of the A button shouldn't delete or remove anything.
            case .confirm: return .confirmCancel
            case .gameMenu: return .gameAction(.play)
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
    /// A target the home screen or the grid scrolls into view once, whatever the input. The screen clears it when it scrolls.
    var revealRequest: Target?
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
        case .openFolder: FileBrowser.open(hub.library.paths.gamesRoot, create: true)
        case .rescan: rescan()
        case .controlMethod, .controllerMouse, .mouseStyle, .playOrientation: stepSetting(target, by: 1)
        case .layoutButton: layout = layout.next
        case .gridButton: push(.grid)
        case .settingsButton: push(.settings)
        case .lockButton: toggleLibraryLock()
        case .play: if let game = heroGame { play(game.id) }
        case .details: if let game = heroGame { push(.game(game.id)) }
        case let .tile(_, id), let .gridTile(id): push(.game(id))
        case .back: back()
        case .pagePlay: if let id = pageGameID { perform(.play, on: id) }
        case .pageArtwork: if let id = pageGameID { perform(.artwork, on: id) }
        case .pageMetadata: if let id = pageGameID { perform(.metadata, on: id) }
        case .pageShowFolder: if let id = pageGameID { perform(.showFolder, on: id) }
        case .pageRemove: if let id = pageGameID { perform(.remove, on: id) }
        case .pageSaves: if let id = pageGameID { sheet = .saves(GameSavesModel(service: hub.saveSync, game: id)) }
        case let .pageScreenshot(index):
            let screenshots = pageScreenshots
            if let id = pageGameID, screenshots.indices.contains(index) {
                sheet = .screenshots(ScreenshotViewer(gameID: id, screenshots: screenshots, index: index))
            }
        case .pageScreenshotsMore: showsAllScreenshots.toggle()
        case let .settingsPage(page):
            // These pages start from the stored settings and the top each time. The Ludusavi page keeps what was typed
            // into it, like the service it shows.
            switch page {
            case .metadata: metadataSettings = MetadataSettingsModel(service: hub.metadata)
            case .lock: lockSettings = LockSettingsModel(lock: hub.lock)
            case .saves, .plugins, .licenses: break
            }
            sheet = .settingsPage(page)
        case .pageEngine: if let id = pageGameID { sheet = .engine(id) }
        case .pageVariant: if let id = pageGameID { sheet = .variant(id) }
        case .pageOrientation: if let id = pageGameID { sheet = .orientation(id) }
        case .pageControls: if let id = pageGameID, offersControlMethod(id) { sheet = .controlMethod(id) }
        case .pageMapping: if let id = pageGameID { sheet = .mapping(ButtonMappingEditor(library: hub.library, gameID: id)) }
        case .pageLock: if let id = pageGameID { perform(.hide, on: id) }
        case let .gameAction(action):
            guard case let .gameMenu(id) = sheet else { return }
            sheet = nil
            perform(action, on: id)
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
            // A controller can't type in the system's text field, so it gets the on-screen keyboard.
            if case let .metadata(search) = sheet {
                if hub.input.lastInputKind == .gamepad { search.openKeyboard() } else { search.editingQuery = true }
            }
        case let .metadataResult(index):
            if case let .metadata(search) = sheet, let results = search.results, results.indices.contains(index) {
                apply(results[index], in: search)
            }
        case let .runtime(packageID, action):
            guard let id = pageGameID, let package = runtimes.missing[id]?.first(where: { $0.id == packageID }) else { return }
            switch action {
            case .install: runtimes.install(package, for: id)
            case .importFolder: runtimes.pickFolder(for: package)
            case .cancel: runtimes.cancel(package)
            }
        case let .fontPackage(packageID):
            guard let package = FontPack.packages.first(where: { $0.id == packageID }) else { return }
            if case .running? = runtimes.phases[packageID] {
                runtimes.cancel(package)
            } else if runtimes.installedFonts.contains(packageID) {
                runtimes.remove(package)
            } else {
                runtimes.install(package, for: nil)
            }
        case .soundFont: pickingSoundFont = true
        case .soundFontReset:
            do {
                try MIDISoundFont.reset()
                soundFontName = nil
                soundFontError = nil
                setFocus(.soundFont)
            } catch {
                soundFontError = error.localizedDescription
            }
        case .licenseAccept, .licenseDecline:
            guard case let .license(request) = sheet else { return }
            request.respond(target == .licenseAccept)
            sheet = nil
        case .confirmAction:
            guard case let .confirm(confirmation) = sheet else { return }
            sheet = nil
            Task { await confirmation.perform() }
        case .confirmCancel: sheet = nil
        case .addLibraryFolder: pickingLibraryFolder = true
        case let .showLibraryFolder(id):
            if let problem = hub.library.folderProblems[id] { return showError("Can't open the folder", problem) }
            if let root = hub.library.root(of: id) { FileBrowser.open(root) }
        case let .removeLibraryFolder(id): confirmRemoving(id)
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
        let library = hub.library
        // Folders that can't be reached now aren't offered.
        let added = library.folders.filter { library.folderProblems[$0.id] == nil }.compactMap { folder in
            library.root(of: folder.id).map { ImportDestination(name: folder.name, url: $0) }
        }
        let builtIn = ImportDestination(name: library.paths.gamesRoot.lastPathComponent, url: library.paths.gamesRoot)
        let model = ArchiveImportModel(gamesRoot: library.paths.gamesRoot, destinations: [builtIn] + added)
        importing = model
        sheet = .importGame(model, initialSelection: urls)
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
        let destination = importing?.destination.url
        importing = nil
        guard let folderName else { return }
        Task {
            await hub.refreshLibrary()
            let library = hub.library
            let folder = library.folders.first { library.root(of: $0.id) == destination }?.id
            guard let id = library.gameID(named: folderName, in: folder) else { return }
            heroID = id
            if let tile = rows().lazy.flatMap(\.items).first(where: { $0.gameID == id }) {
                setFocus(tile)
                // The new game can sort far down the header list, so the library scrolls to it whatever the input.
                revealRequest = tile
            }
        }
    }

    // MARK: Game actions

    /// A library tile's menu, in order. Hide reads Unhide for a hidden game, which shows only while the library is
    /// unlocked.
    func gameActions(_ id: GameID) -> [GameMenuItem] {
        let hidden = hub.library.game(id)?.isLocked == true
        return [
            GameMenuItem(action: .play, title: "Play", systemImage: "play.fill"),
            GameMenuItem(action: .openPage, title: "Open game page", systemImage: "info.circle"),
            GameMenuItem(action: .artwork, title: "Change artwork", systemImage: "photo.on.rectangle"),
            GameMenuItem(action: .metadata, title: "Find metadata", systemImage: "text.magnifyingglass"),
            GameMenuItem(action: .hide, title: hidden ? "Unhide" : "Hide", systemImage: hidden ? "lock.open" : "lock"),
            GameMenuItem(action: .showFolder, title: FileBrowser.openTitle, systemImage: "folder"),
            GameMenuItem(action: .remove, title: "Remove from library", systemImage: "trash"),
        ]
    }

    struct GameMenuItem {
        let action: GameAction
        let title: String
        let systemImage: String
    }

    /// Does what a game page button or a tile menu item asks. Both go through here.
    func perform(_ action: GameAction, on id: GameID) {
        // A hidden game's actions need the library unlocked, however they were reached.
        guard let game = hub.library.game(id), isVisible(game) else { return }
        switch action {
        case .play: play(id)
        case .openPage: if pageGameID != id { push(.game(id)) }
        case .artwork: sheet = .artwork(ArtworkEditor(hub: hub, gameID: id))
        case .metadata: sheet = .metadata(MetadataSearch(hub: hub, gameID: id))
        case .hide: toggleGameLock(id)
        case .showFolder:
            if let problem = hub.library.unavailableReason(id) {
                return showError("Can't open the folder", "\(hub.folderName(of: game.libraryFolder)) can't be reached. \(problem)")
            }
            FileBrowser.open(hub.library.folder(for: id))
        case .remove: confirmDeleting(id)
        }
    }

    /// Asks before deleting the game's folder, saying what goes and what stays on this device.
    private func confirmDeleting(_ id: GameID) {
        guard let game = hub.library.game(id) else { return }
        if let problem = hub.library.unavailableReason(id) {
            return showError("Can't remove \(hub.displayTitle(game))", "Its folder can't be reached, so VNDegen can't delete it. \(problem)")
        }
        let path = SettingsView.displayPath(hub.library.folder(for: id))
        let stays = "Saves VNDegen keeps for the game in its own data folder stay, and so do the game's artwork, play time, and settings, which come back if you put the folder back. Saves the game writes inside its folder go with it."
        #if os(macOS)
        let message = "VNDegen moves the game's folder, \(path), to the Trash. \(stays)"
        let action = "Move to Trash"
        #else
        let message = "VNDegen deletes the game's folder, \(path), and everything in it from this \(UIDevice.current.localizedModel) for good. This can't be undone. \(stays)"
        let action = "Delete folder"
        #endif
        sheet = .confirm(Confirmation(title: "Remove \(hub.displayTitle(game)) from the library?", message: message,
                                      action: action, destructive: true, gameID: id) { [weak self] in
            // The library may have locked since the question was asked.
            guard let self, let game = hub.library.game(id), isVisible(game) else { return }
            do {
                try await hub.library.deleteFolder(of: id)
            } catch {
                showError("Can't remove \(hub.displayTitle(game))", error.localizedDescription)
            }
        })
    }

    // MARK: Library folders

    /// Adds the folder the player picked in Settings, then looks up metadata for its games.
    func addLibraryFolder(_ result: Result<URL, Error>) {
        let url: URL
        do {
            url = try result.get()
        } catch let error as CocoaError where error.code == .userCancelled {
            return
        } catch {
            return showError("Can't add the folder", error.localizedDescription)
        }
        Task {
            do {
                try await hub.addLibraryFolder(url)
            } catch {
                showError("Can't add the folder", error.localizedDescription)
            }
        }
    }

    private func confirmRemoving(_ id: LibraryFolderID) {
        guard let folder = hub.library.folders.first(where: { $0.id == id }) else { return }
        let count = hub.library.games.filter { $0.libraryFolder == id }.count
        let games = count == 1 ? "Its 1 game leaves" : "Its \(count) games leave"
        sheet = .confirm(Confirmation(
            title: "Remove \(folder.name)?",
            message: "\(games) the library. The folder and the files in it stay where they are, at \(SettingsView.displayPath(folder.url)). Adding the folder again brings its games back with their artwork, play time, and settings.",
            action: "Remove folder", destructive: true) { [weak self] in
                guard let self else { return }
                await hub.library.removeFolder(id)
                setFocus(.addLibraryFolder)
            })
    }

    /// Shows `message` in the hub's alert, under `title`.
    func showError(_ title: String, _ message: String) {
        errorTitle = title
        hub.launchError = message
    }

    func closeError() {
        hub.launchError = nil
        errorTitle = nil
    }

    /// Copies the SoundFont the player chose into the app, for engines started after this.
    func installSoundFont(_ result: Result<URL, Error>) {
        do {
            soundFontName = try MIDISoundFont.install(result.get()).lastPathComponent
            soundFontError = nil
        } catch let error as CocoaError where error.code == .userCancelled {
            return
        } catch {
            soundFontError = error.localizedDescription
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
        let paths = hub.library.paths
        return plugin.variants(for: game, folder: hub.library.folder(for: id), dataDirectory: paths.dataDirectory(for: id))
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
        if case .game = route { showsAllScreenshots = false }
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
        // A sheet opened from a tile, such as its menu, isn't under a game page, so it is checked on its own. A hidden
        // game's sheet closes when the library locks, and a deleted game's when it leaves the library.
        if let id = sheet?.gameID, !shown.contains(id) { sheet = nil }
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
        /// Shown in place of the selection mark, for a sheet of actions rather than values.
        var systemImage: String?
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
        case let .gameMenu(id):
            return gameActions(id).map { Choice(target: .gameAction($0.action), title: $0.title, systemImage: $0.systemImage) }
        case .unlock, .artwork, .metadata, .saves, .launchSaves, .settingsPage, .importGame, .mapping, .license, .confirm, .screenshots:
            return []
        }
    }

    func selectedChoice(for sheet: Sheet) -> Target? {
        switch sheet {
        case let .engine(id): .engineOption(hub.library.game(id)?.engineOverride)
        case let .variant(id): .variantOption(variants(id)?.selected)
        case let .orientation(id): .orientationOption(hub.library.game(id)?.orientation)
        case let .controlMethod(id): .controlMethodOption(hub.library.game(id)?.controlMethod)
        case .unlock, .artwork, .metadata, .saves, .launchSaves, .settingsPage, .importGame, .mapping, .license, .confirm, .gameMenu, .screenshots: nil
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
            // The screenshot viewer can open while the saves are checked, and it mustn't stay over the game.
            if case .screenshots? = sheet { sheet = nil }
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

    /// The on-screen keyboard a controller is typing with in the open sheet. It takes every button while it is open.
    var keyboard: OnScreenKeyboard? {
        switch sheet {
        case let .metadata(search)?: search.keyboard
        case let .importGame(model, _)?: model.keyboard
        case let .saves(model)?: model.keyboard
        case .settingsPage(.saves)?: saveSettings.keyboard
        case .settingsPage(.metadata)?: metadataSettings.keyboard
        default: nil
        }
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
        defer { logState() }
        if case .unlock = sheet {
            unlockView?.handle(event)
            return
        }
        guard case let .button(button, pressed: true) = event else { return }
        // The alert covers everything else, and it shows no focus, so A and B both close it.
        if hub.launchError != nil {
            if button == .confirm || button == .cancel { closeError() }
            return
        }
        // A system file picker covers the hub and takes no controller input, so presses mustn't reach the rows behind
        // it. B closes it.
        if pickingSoundFont || runtimes.pickingFolder || pickingLibraryFolder {
            if button == .cancel {
                pickingSoundFont = false
                runtimes.pickingFolder = false
                pickingLibraryFolder = false
            }
            return
        }
        if let keyboard {
            keyboard.handle(button)
            return
        }
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
        case .settingsPage(.metadata)?:
            if !metadataSettings.handle(button), button == .cancel { back() }
            return
        case .settingsPage(.lock)?:
            if !lockSettings.handle(button), button == .cancel { back() }
            return
        case .settingsPage(.licenses)?, .settingsPage(.plugins)?, .license?:
            // Licenses and the plugin list are text to read: up and down scroll a little, the shoulder buttons a page.
            switch button {
            case .up, .down: return pager.scroll(pages: button == .up ? -0.3 : 0.3)
            case .pageUp, .pageDown: return pager.scroll(pages: button == .pageUp ? -1 : 1)
            default: break
            }
        case let .mapping(editor)?:
            if !editor.handle(button), button == .cancel { back() }
            return
        case let .screenshots(viewer)?:
            switch button {
            case .left, .pageUp: viewer.step(-1)
            case .right, .pageDown: viewer.step(1)
            case .confirm, .cancel: back()
            default: break
            }
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
        // X opens the focused tile's menu. Nothing else in the library uses it.
        case .dash: if sheet == nil, let id = focus?.gameID { sheet = .gameMenu(id) }
        case .pageUp: page(false)
        case .pageDown: page(true)
        case .hub: break
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

#if DEBUG
extension Navigator {
    /// With `-logNavigation YES`, logs where the hub is after each controller press, so a script driving the DEBUG hooks
    /// can check the result without a screenshot.
    func logState() {
        guard UserDefaults.standard.bool(forKey: "logNavigation") else { return }
        let sheet = self.sheet.map { "\($0.id)" } ?? "none"
        let route = self.route.map { "\($0)" } ?? "home"
        let keyboard = keyboard.map { "\($0.title):\($0.focusedKey):\($0.isSecure ? "secure" : $0.text)" } ?? "none"
        var detail = ""
        if case let .mapping(editor)? = self.sheet {
            detail = " mapping=\(editor.picking.map { "\($0)" } ?? "list"):\(editor.focus.map { "\($0)" } ?? "none")"
        }
        if case let .screenshots(viewer)? = self.sheet { detail = " screenshot=\(viewer.index)" }
        NSLog("HUBNAV route=%@ sheet=%@ focus=%@ keyboard=%@ alert=%@%@", route, sheet, focus.map { "\($0)" } ?? "none", keyboard,
              hub.launchError ?? "none", detail)
    }
}
#else
extension Navigator {
    func logState() {}
}
#endif
