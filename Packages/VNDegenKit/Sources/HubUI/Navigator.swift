import Foundation
import Observation
import VNCore
import Lock

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

    enum Sheet: Identifiable {
        case unlock
        case engine(GameID)
        case artwork(GameID)
        case metadata(GameID)

        var id: String {
            switch self {
            case .unlock: "unlock"
            case let .engine(id): "engine:\(id)"
            case let .artwork(id): "artwork:\(id)"
            case let .metadata(id): "metadata:\(id)"
            }
        }
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
        case gridButton, lockButton, settingsButton
        /// Hero buttons. They act on `heroGame`.
        case play, details
        case tile(Shelf, GameID)
        case gridTile(GameID)
        case back
        case pagePlay, pageArtwork, pageMetadata, pageLock, pageEngine
        case engineOption(EngineID?)
    }

    struct Row {
        let id: String
        let items: [Target]
        /// Shelves remember their column when focus leaves, like Big Picture. Grid rows keep the column instead.
        var remembersColumn = true
    }

    let hub: Hub
    private(set) var path: [Route] = []
    var sheet: Sheet? { didSet { syncInputMode() } }
    private(set) var focus: Target?
    /// The game shown in the home hero: the last game whose tile had focus.
    private var heroID: GameID?
    /// Set by the grid view from its width.
    var gridColumns = 4
    var artworkSlot: ArtworkSlot = .capsule
    @ObservationIgnored private var columnMemory: [String: Int] = [:]
    /// The unlock view shown in the sheet, kept so controller events reach the same instance.
    @ObservationIgnored private(set) var unlockView: UnlockView?

    init(hub: Hub) { self.hub = hub }

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

    func rows() -> [Row] {
        if let sheet {
            switch sheet {
            case let .engine(id): return engineOptions(for: id).map { Row(id: "engine-\($0.id?.rawValue ?? "auto")", items: [.engineOption($0.id)]) }
            case .unlock, .artwork, .metadata: return []
            }
        }
        switch route {
        case nil:
            let bar: [Target] = visibleGames.isEmpty ? [.lockButton, .settingsButton] : [.gridButton, .lockButton, .settingsButton]
            var rows = [Row(id: "bar", items: bar)]
            if heroGame != nil { rows.append(Row(id: "hero", items: [.play, .details])) }
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
            return [
                Row(id: "page-back", items: [.back]),
                Row(id: "page-actions", items: [.pagePlay, .pageArtwork, .pageMetadata, .pageLock]),
                Row(id: "page-engine", items: [.pageEngine]),
            ]
        case .settings:
            return [Row(id: "settings-back", items: [.back])]
        }
    }

    private var defaultFocus: Target? {
        if case let .engine(id) = sheet { return .engineOption(hub.library.game(id)?.engineOverride) }
        switch route {
        case nil:
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
    func isFocused(_ target: Target) -> Bool {
        focus == target && hub.input.lastInputKind != .touch
    }

    /// Pointer hover on Mac and iPad moves focus without activating.
    func hover(_ target: Target) {
        guard sheet == nil else { return }
        hub.input.noteInput(.keyboardMouse)
        if focus != target { setFocus(target) }
    }

    /// Tap or click: focus and activate in one step.
    func select(_ target: Target) {
        #if os(macOS)
        hub.input.noteInput(.keyboardMouse)
        #else
        hub.input.noteInput(.touch)
        #endif
        setFocus(target)
        activate(target)
    }

    // MARK: Actions

    func activate(_ target: Target) {
        switch target {
        case .gridButton: push(.grid)
        case .settingsButton: push(.settings)
        case .lockButton: toggleLibraryLock()
        case .play: if let game = heroGame { hub.launch(game.id) }
        case .details: if let game = heroGame { push(.game(game.id)) }
        case let .tile(_, id), let .gridTile(id): push(.game(id))
        case .back: back()
        case .pagePlay: if let id = pageGameID { hub.launch(id) }
        case .pageArtwork: if let id = pageGameID { sheet = .artwork(id) }
        case .pageMetadata: if let id = pageGameID { sheet = .metadata(id) }
        case .pageEngine: if let id = pageGameID { sheet = .engine(id) }
        case .pageLock: if let id = pageGameID { toggleGameLock(id) }
        case let .engineOption(engine):
            if case let .engine(id) = sheet { hub.library.update(id) { $0.engineOverride = engine } }
            sheet = nil
        }
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
            unlockView = UnlockView(lock: hub.lock) { [weak self] _ in
                guard let self else { return }
                if case .unlock = self.sheet { self.sheet = nil }
                self.unlockView = nil
                self.revalidate()
            }
            sheet = .unlock
        }
    }

    /// Locks the library and drops any screen that shows a locked game.
    func relock() {
        hub.lock.lock()
        revalidate()
    }

    private func toggleGameLock(_ id: GameID) {
        hub.library.update(id) { $0.isLocked.toggle() }
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
        // Until the user steers with a controller, keep the default focus current as the library loads.
        if hub.input.lastInputKind == .touch || focus.map({ position(of: $0, in: rows()) == nil }) ?? true { setFocus(defaultFocus) }
    }

    // MARK: Engines

    struct EngineOption: Identifiable {
        let id: EngineID?
        let title: String
        let detail: String?
    }

    /// "Automatic (detected: X)" first, then every engine that runs on this platform.
    func engineOptions(for id: GameID) -> [EngineOption] {
        let game = hub.library.game(id)
        let detected = game?.detected.flatMap { hub.library.plugin(for: $0.engine)?.descriptor.displayName }
        let automatic = EngineOption(id: nil, title: "Automatic", detail: "Detected: \(detected ?? "none")")
        let manual = hub.library.engines
            .map(\.descriptor)
            .filter { $0.platforms.contains(.current) }
            .map { EngineOption(id: $0.id, title: $0.displayName, detail: nil) }
        return [automatic] + manual
    }

    // MARK: Input

    /// Takes over controller input for the hub. Called whenever the library is on screen.
    func attachInput() {
        hub.input.onEvent = { [weak self] event in self?.handle(event) }
        hub.input.repeatsDirections = true
        syncInputMode()
    }

    /// Text fields on the settings screen and in the metadata search need raw keyboard input.
    private func syncInputMode() {
        guard case .library = hub.screen else { return }
        let typing: Bool
        switch (sheet, route) {
        case (.metadata?, _), (nil, .settings?): typing = true
        default: typing = false
        }
        hub.input.translatesKeyboard = !typing
    }

    func handle(_ event: ControllerEvent) {
        if case .unlock = sheet {
            unlockView?.handle(event)
            return
        }
        guard case let .button(button, pressed: true) = event else { return }
        if case .artwork = sheet {
            switch button {
            case .left, .pageUp: cycleArtworkSlot(-1)
            case .right, .pageDown: cycleArtworkSlot(1)
            case .cancel: back()
            default: break
            }
            return
        }
        switch button {
        case .up: move(.up)
        case .down: move(.down)
        case .left: move(.left)
        case .right: move(.right)
        case .confirm: if let focus { activate(focus) } else { setFocus(defaultFocus) }
        case .cancel: back()
        case .menu: if sheet == nil, route != .settings { push(.settings) }
        case .pageUp: page(false)
        case .pageDown: page(true)
        case .dash, .hub: break
        }
    }

    private func cycleArtworkSlot(_ step: Int) {
        let slots = ArtworkSlot.allCases
        let index = slots.firstIndex(of: artworkSlot) ?? 0
        artworkSlot = slots[(index + step + slots.count) % slots.count]
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
