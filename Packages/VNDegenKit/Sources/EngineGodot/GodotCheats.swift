import VNCore

/// Game speed, the one cheat every Godot game can have. Godot games keep their gold, items and party in their own
/// scripts, each in its own way, so there is nothing else to offer.
final class GodotCheats: CheatSession {
    private let setSpeed: (Int) -> Void

    init(setSpeed: @escaping (Int) -> Void) {
        self.setSpeed = setSpeed
    }

    let features: CheatFeatures = .speed
    let variableRange = 0...0

    /// Speed works on the title screen too, so the page never asks the player to start a game first.
    func state() async throws -> CheatState { CheatState(inGame: true) }

    /// The page offers no actions for a game with speed only.
    func perform(_ action: CheatAction) async throws {}

    func apply(_ settings: CheatSettings) {
        setSpeed(settings.speed)
    }
}
