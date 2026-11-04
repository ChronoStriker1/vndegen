import Foundation
import Observation
import VNCore

/// A layout the library offers: one of VNDegen's own, or one the player saved.
nonisolated public struct ControllerLayout: Identifiable, Equatable, Sendable {
    public enum ID: Hashable, Sendable {
        case keyboardAndMouse, mouse, rpgMaker, gamepad
        case saved(UUID)
    }

    public let id: ID
    public var name: String
    public var mapping: ButtonMapping

    public static let builtIns = [
        ControllerLayout(id: .keyboardAndMouse, name: "Keyboard and mouse", mapping: .keyboardAndMouse),
        ControllerLayout(id: .mouse, name: "Mouse", mapping: .mouse),
        ControllerLayout(id: .rpgMaker, name: "RPG Maker controls", mapping: .rpgMaker),
        ControllerLayout(id: .gamepad, name: "Xbox gamepad", mapping: ButtonMapping()),
    ]
}

/// The controller layouts a game can use. Picking one copies its buttons into `Game.buttonMapping`, so a game never
/// depends on a layout still existing, and which layout a game uses is worked out from its buttons.
@Observable
public final class LayoutLibrary {
    /// The layouts the player saved, oldest first.
    public private(set) var saved: [ControllerLayout] = []
    @ObservationIgnored private let support: URL
    private var file: URL { support.appendingPathComponent("controller-layouts.json") }

    public init(paths: LibraryPaths) {
        support = paths.support
        guard FileManager.default.fileExists(atPath: file.path) else { return }
        do {
            let stored = try JSONDecoder().decode([Stored].self, from: Data(contentsOf: file))
            saved = stored.map { ControllerLayout(id: .saved($0.id), name: $0.name, mapping: ButtonMapping($0.buttons)) }
        } catch {
            // The next save would replace a file this version can't read, such as one a newer version wrote, so it moves
            // aside to stay recoverable.
            try? FileManager.default.moveItem(at: file, to: support.appendingPathComponent("controller-layouts-unreadable-\(UUID()).json"))
        }
    }

    public var all: [ControllerLayout] { ControllerLayout.builtIns + saved }

    public func layout(_ id: ControllerLayout.ID) -> ControllerLayout? { all.first { $0.id == id } }

    /// The layout a game with these buttons shows: the first in the library with the same buttons. Engine defaults are
    /// built in and come first, so a game following its engine's default never shows a saved layout.
    public func shown(for mapping: ButtonMapping) -> ControllerLayout? { all.first { $0.mapping == mapping } }

    /// Saves a new layout, unless `name` is blank.
    @discardableResult
    public func add(name: String, mapping: ButtonMapping) -> ControllerLayout.ID? {
        let name = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !name.isEmpty else { return nil }
        let id = ControllerLayout.ID.saved(UUID())
        saved.append(ControllerLayout(id: id, name: name, mapping: mapping))
        store()
        return id
    }

    /// Renames a saved layout, unless `name` is blank.
    public func rename(_ id: ControllerLayout.ID, to name: String) {
        let name = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !name.isEmpty, let index = saved.firstIndex(where: { $0.id == id }) else { return }
        saved[index].name = name
        store()
    }

    /// Deletes a saved layout. Games that used it keep their buttons.
    public func delete(_ id: ControllerLayout.ID) {
        saved.removeAll { $0.id == id }
        store()
    }

    /// Changes a saved layout's buttons, and those of every game that shows it. A game that matches it but shows an
    /// earlier layout stays as it is.
    public func update(_ id: ControllerLayout.ID, mapping: ButtonMapping, in library: LibraryStore) {
        guard let index = saved.firstIndex(where: { $0.id == id }) else { return }
        let games = library.allGames.filter { shown(for: ButtonMapping(for: $0, in: library))?.id == id }
        saved[index].mapping = mapping
        store()
        for game in games {
            let stored = mapping.stored(default: ButtonMapping.defaultLayout(for: game, in: library))
            library.update(game.id) { $0.buttonMapping = stored }
        }
    }

    private func store() {
        let stored = saved.compactMap { layout -> Stored? in
            guard case let .saved(id) = layout.id else { return nil }
            return Stored(id: id, name: layout.name, buttons: layout.mapping.overrides)
        }
        do {
            try FileManager.default.createDirectory(at: support, withIntermediateDirectories: true)
            try JSONEncoder().encode(stored).write(to: file, options: .atomic)
        } catch {
            assertionFailure("controller layouts save failed: \(error)")
        }
    }

    /// A saved layout in the file, with its buttons as `Game.buttonMapping` stores them.
    nonisolated private struct Stored: Codable {
        let id: UUID
        let name: String
        let buttons: [ControlInput: ControlAction]
    }
}
