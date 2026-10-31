import CRGSS
import Foundation
import VNCore

/// Cheats through rgss-cheats.rb, which the runtime runs on the game thread while the game waits for its next frame.
final class RGSSCheats: CheatSession {
    let features: CheatFeatures = [.gold, .restoreParty, .items, .keepPartyHealthy, .noEncounters, .walkThroughWalls,
                                   .speed, .experience, .variables, .switches]

    /// What RPG Maker's own event commands let a variable hold.
    let variableRange = -99_999_999...99_999_999

    /// Loads the cheats into a game that just started, with the settings it starts with. They run at its first frame and
    /// wrap the game's methods once its scripts define them.
    static func start(with settings: CheatSettings) {
        vnrgss_set_speed(Int32(settings.speed))
        guard let url = Bundle.module.url(forResource: "rgss-cheats", withExtension: "rb"),
              let source = try? String(contentsOf: url, encoding: .utf8) else {
            rgssLog.error("cheats: rgss-cheats.rb is missing from the app bundle")
            return
        }
        let request = vnrgss_run_ruby(source + "\nVNDegenCheats.apply(\(ruby(settings)))")
        Task {
            do {
                _ = try await result(of: request)
            } catch {
                rgssLog.error("cheats did not load: \(error.localizedDescription, privacy: .public)")
            }
        }
    }

    #if DEBUG
    /// `-rgss-ruby <file>` runs a Ruby file in the game once its scripts have loaded, for scripted checks, and logs
    /// what it gives ("rgss-ruby" in the app log).
    static func runDebugScript() {
        guard let path = UserDefaults.standard.string(forKey: "rgss-ruby"),
              let source = try? String(contentsOfFile: path, encoding: .utf8) else { return }
        let request = vnrgss_run_ruby(source)
        Task {
            do {
                let result = try await result(of: request)
                rgssLog.notice("rgss-ruby: \(result, privacy: .public)")
            } catch {
                rgssLog.error("rgss-ruby failed: \(error.localizedDescription, privacy: .public)")
            }
        }
    }
    #endif

    func state() async throws -> CheatState {
        try CheatState(json: Data(try await Self.run("VNDegenCheats.state").utf8))
    }

    func perform(_ action: CheatAction) async throws {
        guard try await Self.run("VNDegenCheats.perform(\(Self.ruby(action)))") == "true" else { throw RGSSCheatError.notInGame }
    }

    func apply(_ settings: CheatSettings) {
        vnrgss_set_speed(Int32(settings.speed))
        Task {
            do {
                _ = try await Self.run("VNDegenCheats.apply(\(Self.ruby(settings)))")
            } catch {
                rgssLog.error("cheat settings: \(error.localizedDescription, privacy: .public)")
            }
        }
    }

    private static func run(_ ruby: String) async throws -> String {
        try await result(of: vnrgss_run_ruby(ruby))
    }

    /// Waits for the game thread to run a request, which a game in a long load gets to at its next frame. A request
    /// can't be withdrawn once queued, so this waits for its result even when cancelled, rather than report a change
    /// that may still happen. The game ending finishes every request.
    private static func result(of request: Int) async throws -> String {
        guard request != 0 else { throw RGSSCheatError.notRunning }
        while true {
            var failed = false
            if let text = vnrgss_ruby_result(request, &failed) {
                defer { free(text) }
                let result = String(cString: text)
                if failed { throw RGSSCheatError.ruby(result) }
                return result
            }
            // A cancelled task's own sleep returns at once, so the wait runs in a task of its own.
            await Task.detached { try? await Task.sleep(for: .milliseconds(10)) }.value
        }
    }

    // MARK: Ruby literals

    nonisolated static func ruby(_ settings: CheatSettings) -> String {
        "{'keepsPartyHealthy' => \(settings.keepsPartyHealthy), 'noEncounters' => \(settings.noEncounters), "
            + "'walksThroughWalls' => \(settings.walksThroughWalls), 'speed' => \(settings.speed), "
            + "'experienceRate' => \(settings.experienceRate)}"
    }

    nonisolated static func ruby(_ action: CheatAction) -> String {
        switch action {
        case let .addGold(amount): "{'type' => 'gold', 'amount' => \(amount)}"
        case .restoreParty: "{'type' => 'restore'}"
        case let .addItem(item, count): "{'type' => 'item', 'kind' => '\(item.kind.rawValue)', 'id' => \(item.number), 'count' => \(count)}"
        case let .setVariable(id, value):
            switch value {
            case let .integer(number): "{'type' => 'variable', 'id' => \(id), 'value' => \(number)}"
            case let .text(text): "{'type' => 'variable', 'id' => \(id), 'value' => \(ruby(text))}"
            // The page never offers to edit these.
            case .other: "{'type' => 'none'}"
            }
        case let .setSwitch(id, on): "{'type' => 'switch', 'id' => \(id), 'on' => \(on)}"
        }
    }

    /// Text a player typed, as Ruby that makes the same string from its characters' numbers, so no text can end the
    /// literal or run as code. Ruby interpolates `#{` in a double-quoted string even when the `#` is an escape.
    nonisolated static func ruby(_ text: String) -> String {
        "[\(text.unicodeScalars.map { String($0.value) }.joined(separator: ","))].pack('U*')"
    }
}

nonisolated enum RGSSCheatError: LocalizedError {
    case notRunning, notInGame
    case ruby(String)

    var errorDescription: String? {
        switch self {
        case .notRunning: "The game isn't running."
        case .notInGame: "Start or load a game first."
        case let .ruby(message): message
        }
    }
}
