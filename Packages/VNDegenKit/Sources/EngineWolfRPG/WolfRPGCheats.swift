import CWolfRPG
import Foundation
import VNCore

/// Cheats through the runtime's own variables and databases (vnwolf.h). Wolf RPG Editor has no gold, items or party of
/// its own, so those cheats are offered for games that keep them where the editor's standard systems do.
final class WolfRPGCheats: CheatSession {
    /// The running engine, or nil once the game has ended.
    private let engine: () -> OpaquePointer?

    init(engine: @escaping () -> OpaquePointer?) {
        self.engine = engine
    }

    var features: CheatFeatures {
        var features: CheatFeatures = [.variables, .walkThroughWalls, .speed]
        if let engine = engine(), vnwolf_cheat_has_party(engine) { features.formUnion([.gold, .items, .restoreParty]) }
        return features
    }

    /// Wolf's variables are 32-bit.
    let variableRange = Int(Int32.min)...Int(Int32.max)

    func state() async throws -> CheatState {
        guard let engine = engine(), let json = vnwolf_cheat_state(engine) else { throw WolfRPGCheatError.notRunning }
        return try CheatState(json: Data(String(cString: json).utf8))
    }

    func perform(_ action: CheatAction) async throws {
        guard let engine = engine() else { throw WolfRPGCheatError.notRunning }
        let done = switch action {
        case let .addGold(amount): vnwolf_cheat_add_gold(engine, Int32(clamping: amount))
        case .restoreParty: vnwolf_cheat_restore_party(engine)
        case let .addItem(item, count):
            vnwolf_cheat_add_item(engine, Self.kinds.firstIndex(of: item.kind).map(Int32.init) ?? -1, Int32(clamping: item.number),
                                  Int32(clamping: count))
        case let .setVariable(id, .integer(value)): vnwolf_cheat_set_variable(engine, Int32(clamping: id), Int32(clamping: value))
        // Wolf's Variables tab lists only numbers, and Wolf has no switches.
        case .setVariable, .setSwitch: false
        }
        guard done else { throw WolfRPGCheatError.notInGame }
    }

    func apply(_ settings: CheatSettings) {
        guard let engine = engine() else { return }
        vnwolf_set_speed(engine, Int32(settings.speed))
        vnwolf_cheat_walk_through_walls(engine, settings.walksThroughWalls)
    }

    /// vnwolf_cheat_add_item's kinds, in order.
    private static let kinds: [CheatItem.Kind] = [.item, .weapon, .armor]
}

nonisolated enum WolfRPGCheatError: LocalizedError {
    case notRunning, notInGame

    var errorDescription: String? {
        switch self {
        case .notRunning: "The game isn't running."
        case .notInGame: "Start or load a game first."
        }
    }
}
