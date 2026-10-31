import CSystem3
import Foundation
import VNCore

/// Cheats through the System 3 runtime's own variables (vns3.h). The games name their variables VAR0000 and on, or
/// not at all, so nothing says which one holds gold: the Variables tab's find by value is how the player finds it.
final class System3Cheats: CheatSession {
    /// The running engine, or nil before it starts and after the game has ended.
    private let engine: () -> OpaquePointer?

    init(engine: @escaping () -> OpaquePointer?) {
        self.engine = engine
    }

    let features: CheatFeatures = [.variables, .speed]

    /// System 3's variables are 16-bit, without a sign.
    let variableRange = 0...65535

    /// The variables live from the title screen on, and the game's own scripts set them when a game starts or loads.
    func state() async throws -> CheatState {
        guard let engine = engine() else { throw System3CheatError.notRunning }
        let variables = (0..<Int(vns3_variable_count(engine))).map { index in
            CheatVariable(id: index, name: vns3_variable_name(engine, Int32(index)).map { String(cString: $0) } ?? "",
                          value: .integer(Int(vns3_variable(engine, Int32(index)))))
        }
        return CheatState(inGame: true, variables: variables)
    }

    func perform(_ action: CheatAction) async throws {
        guard let engine = engine() else { throw System3CheatError.notRunning }
        // The Variables tab is the only place System 3 offers changes, and its values are whole numbers.
        guard case let .setVariable(id, .integer(value)) = action else { return }
        guard vns3_set_variable(engine, Int32(clamping: id), Int32(clamping: value)) else { throw System3CheatError.noVariable(id) }
    }

    func apply(_ settings: CheatSettings) {
        guard let engine = engine() else { return }
        vns3_set_speed(engine, Int32(settings.speed))
    }
}

nonisolated enum System3CheatError: LocalizedError {
    case notRunning, noVariable(Int)

    var errorDescription: String? {
        switch self {
        case .notRunning: "The game isn't running."
        case let .noVariable(id): "The game has no variable \(id)."
        }
    }
}
