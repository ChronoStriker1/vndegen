import Foundation
import VNCore
import WebRuntime

/// Cheats through rpgmaker-cheats.js, which works on the game's own objects in its page.
final class RPGMakerCheats: CheatSession {
    /// Weak: the page's session owns these cheats.
    private weak var page: (any GamePage)?
    /// rpgmaker-cheats.js as it is in the app.
    private let template: String
    /// The script the page starts with, with the settings it starts with.
    private var script: String

    /// `script` is what `script(_:settings:)` made of `template` for the page's first load.
    init(page: any GamePage, template: String, script: String) {
        self.page = page
        self.template = template
        self.script = script
    }

    let features: CheatFeatures = [.gold, .restoreParty, .items, .keepPartyHealthy, .noEncounters, .walkThroughWalls,
                                   .speed, .experience, .variables, .switches]

    /// JavaScript's safe integers. Past them a number no longer holds every whole value.
    let variableRange = -9_007_199_254_740_991...9_007_199_254_740_991

    func state() async throws -> CheatState {
        guard let json = try await call("return __vndegen.cheats.state()") as? String else { throw RPGMakerCheatError.noAnswer }
        return try CheatState(json: Data(json.utf8))
    }

    func perform(_ action: CheatAction) async throws {
        guard try await call("return __vndegen.cheats.perform(action)", arguments: ["action": Self.arguments(for: action)]) as? Bool == true
        else { throw RPGMakerCheatError.notInGame }
    }

    /// The page that's running gets the settings now. A reload, by the game or after WebKit restarted the page, starts
    /// with them too.
    func apply(_ settings: CheatSettings) {
        if let next = try? Self.script(template, settings: settings) {
            page?.replaceStartScript(script, with: next)
            script = next
        }
        Task {
            do {
                _ = try await call("__vndegen.cheats.apply(settings)", arguments: ["settings": Self.arguments(for: settings)])
            } catch {
                rpgLog.error("cheat settings: \(error.localizedDescription, privacy: .public)")
            }
        }
    }

    private func call(_ body: String, arguments: [String: Any] = [:]) async throws -> Any? {
        guard let page else { throw CancellationError() }
        return try await page.callFunction(body, arguments: arguments)
    }

    /// What the script reads as its settings, in the page and in place of `__VNDEGEN_CHEATS__`.
    nonisolated static func arguments(for settings: CheatSettings) -> [String: Any] {
        ["keepsPartyHealthy": settings.keepsPartyHealthy, "noEncounters": settings.noEncounters,
         "walksThroughWalls": settings.walksThroughWalls, "speed": settings.speed, "experienceRate": settings.experienceRate]
    }

    nonisolated static func arguments(for action: CheatAction) -> [String: Any] {
        switch action {
        case let .addGold(amount): ["type": "gold", "amount": amount]
        case .restoreParty: ["type": "restore"]
        case let .addItem(item, count): ["type": "item", "kind": item.kind.rawValue, "id": item.number, "count": count]
        case let .setVariable(id, value):
            switch value {
            case let .integer(number): ["type": "variable", "id": id, "value": number]
            case let .text(text): ["type": "variable", "id": id, "value": text]
            // The page never offers to edit these.
            case .other: ["type": "none"]
            }
        case let .setSwitch(id, on): ["type": "switch", "id": id, "on": on]
        }
    }

    /// The script with `settings` in place.
    nonisolated static func script(_ source: String, settings: CheatSettings) throws -> String {
        let json = try JSONSerialization.data(withJSONObject: arguments(for: settings), options: [.sortedKeys])
        return source.replacingOccurrences(of: "__VNDEGEN_CHEATS__", with: String(decoding: json, as: UTF8.self))
    }

}

nonisolated enum RPGMakerCheatError: LocalizedError {
    case noAnswer, notInGame

    var errorDescription: String? {
        switch self {
        case .noAnswer: "The game didn't answer."
        case .notInGame: "Start or load a game first."
        }
    }
}
