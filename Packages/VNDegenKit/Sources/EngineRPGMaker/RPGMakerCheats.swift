import Foundation
import VNCore
import WebRuntime

/// Cheats through rpgmaker-cheats.js, which works on the game's own objects in its page.
final class RPGMakerCheats: CheatSession {
    /// Weak: the page's session owns these cheats.
    private weak var page: (any GamePage)?

    init(page: any GamePage) {
        self.page = page
    }

    let features: CheatFeatures = [.gold, .restoreParty, .items, .keepPartyHealthy, .noEncounters, .walkThroughWalls,
                                   .speed, .experience, .variables, .switches]

    func state() async throws -> CheatState {
        guard let json = try await call("return __vndegen.cheats.state()") as? String else { throw RPGMakerCheatError.noAnswer }
        return try Self.state(fromJSON: Data(json.utf8))
    }

    func perform(_ action: CheatAction) async throws {
        guard try await call("return __vndegen.cheats.perform(action)", arguments: ["action": Self.arguments(for: action)]) as? Bool == true
        else { throw RPGMakerCheatError.notInGame }
    }

    func apply(_ settings: CheatSettings) {
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

    nonisolated static func state(fromJSON json: Data) throws -> CheatState {
        let snapshot = try JSONDecoder().decode(Snapshot.self, from: json)
        return CheatState(
            inGame: snapshot.inGame, gold: snapshot.gold, currencyUnit: snapshot.currencyUnit ?? "",
            party: (snapshot.party ?? []).map(\.member), items: (snapshot.items ?? []).compactMap(\.item),
            variables: (snapshot.variables ?? []).map { CheatVariable(id: $0.id, name: $0.name, value: $0.value.value) },
            switches: (snapshot.switches ?? []).map { CheatSwitch(id: $0.id, name: $0.name, isOn: $0.on) })
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

/// What rpgmaker-cheats.js's `state()` returns. Lists come as arrays, which keep thousands of variables short.
nonisolated private struct Snapshot: Decodable {
    let inGame: Bool
    let gold: Int?
    let currencyUnit: String?
    let party: [Member]?
    let items: [Item]?
    let variables: [Variable]?
    let switches: [Switch]?

    /// Name, HP, max HP, MP, max MP. Plugins can make the numbers fractional.
    struct Member: Decodable {
        let member: CheatPartyMember

        init(from decoder: any Decoder) throws {
            var fields = try decoder.unkeyedContainer()
            let name = try fields.decode(String.self)
            let numbers = try (0..<4).map { _ in Int(try fields.decode(Double.self).rounded()) }
            member = CheatPartyMember(name: name, hp: numbers[0], maxHP: numbers[1], mp: numbers[2], maxMP: numbers[3])
        }
    }

    /// Kind, database number, name, number owned.
    struct Item: Decodable {
        let item: CheatItem?

        init(from decoder: any Decoder) throws {
            var fields = try decoder.unkeyedContainer()
            let kind = CheatItem.Kind(rawValue: try fields.decode(String.self))
            let number = try fields.decode(Int.self)
            let name = try fields.decode(String.self)
            let owned = try fields.decode(Int.self)
            item = kind.map { CheatItem(kind: $0, number: number, name: name, owned: owned) }
        }
    }

    struct Variable: Decodable {
        let id: Int
        let name: String
        let value: Value

        init(from decoder: any Decoder) throws {
            var fields = try decoder.unkeyedContainer()
            id = try fields.decode(Int.self)
            name = try fields.decode(String.self)
            value = try fields.decode(Value.self)
        }
    }

    /// A whole number, `{s: text}`, or `{o: description}`.
    struct Value: Decodable {
        let value: CheatValue

        private enum Keys: String, CodingKey { case s, o }

        init(from decoder: any Decoder) throws {
            if let number = try? decoder.singleValueContainer().decode(Int.self) {
                value = .integer(number)
                return
            }
            let keyed = try decoder.container(keyedBy: Keys.self)
            if let text = try keyed.decodeIfPresent(String.self, forKey: .s) {
                value = .text(text)
            } else {
                value = .other(try keyed.decodeIfPresent(String.self, forKey: .o) ?? "")
            }
        }
    }

    struct Switch: Decodable {
        let id: Int
        let name: String
        let on: Bool

        init(from decoder: any Decoder) throws {
            var fields = try decoder.unkeyedContainer()
            id = try fields.decode(Int.self)
            name = try fields.decode(String.self)
            on = try fields.decode(Bool.self)
        }
    }
}
