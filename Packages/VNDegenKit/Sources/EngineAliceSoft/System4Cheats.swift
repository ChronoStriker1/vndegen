import CSystem4
import Foundation
import VNCore

/// Cheats through the System 4 runtime's own globals (vns4.h). Gold is offered for the library games whose globals
/// say which one holds the party's money; for the rest, the Variables tab's find by value finds it.
final class System4Cheats: CheatSession {
    /// The running engine, or nil before it starts and after the game has ended.
    private let engine: () -> OpaquePointer?

    init(engine: @escaping () -> OpaquePointer?) {
        self.engine = engine
    }

    var features: CheatFeatures {
        var features: CheatFeatures = [.variables, .speed]
        if let engine = engine(), vns4_cheat_has_gold(engine) { features.insert(.gold) }
        return features
    }

    /// System 4's whole numbers are 32-bit.
    let variableRange = Int(Int32.min)...Int(Int32.max)

    /// The globals live from the title screen on, and the game's own scripts set them when a game starts or loads.
    func state() async throws -> CheatState {
        guard let engine = engine() else { throw System4CheatError.notRunning }
        guard let json = vns4_cheat_state(engine) else { throw System4CheatError.busy }
        return try CheatState(json: Data(String(cString: json).utf8))
    }

    func perform(_ action: CheatAction) async throws {
        guard let engine = engine() else { throw System4CheatError.notRunning }
        switch action {
        case let .addGold(amount):
            guard vns4_cheat_add_gold(engine, Int32(clamping: amount)) else { throw System4CheatError.busy }
        case let .setVariable(id, .integer(value)):
            switch Int(vns4_cheat_set_global(engine, Int32(clamping: id), Int32(clamping: value))) {
            case VNS4_CHEAT_OK: break
            case VNS4_CHEAT_BOOL: throw System4CheatError.yesOrNo(id)
            case VNS4_CHEAT_NOT_NUMBER: throw System4CheatError.notANumber(id)
            default: throw System4CheatError.busy
            }
        // The page offers nothing else for System 4.
        case .setVariable, .setSwitch, .addItem, .restoreParty: break
        }
    }

    func apply(_ settings: CheatSettings) {
        guard let engine = engine() else { return }
        vns4_set_speed(engine, Int32(settings.speed))
    }
}

nonisolated enum System4CheatError: LocalizedError {
    case notRunning, busy, yesOrNo(Int), notANumber(Int)

    var errorDescription: String? {
        switch self {
        case .notRunning: "The game isn't running."
        case .busy: "The game is busy. Try again in a moment."
        case let .yesOrNo(id): "Global \(id) is a yes-or-no value: 0 or 1."
        case let .notANumber(id): "Global \(id) isn't a whole number."
        }
    }
}
