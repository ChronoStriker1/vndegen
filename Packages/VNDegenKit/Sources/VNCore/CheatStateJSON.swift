import Foundation

extension CheatState {
    /// Reads the JSON the engines' cheat scripts give: `{"inGame": true, "gold": 50, "currencyUnit": "G", "party": [[name,
    /// hp, maxHP, mp, maxMP]], "items": [[kind, number, name, owned]], "variables": [[id, name, value]], "switches":
    /// [[id, name, on]]}`, where a value is a whole number, `{"s": text}`, or `{"o": description}`. Only `inGame` is
    /// required.
    public init(json: Data) throws {
        let snapshot = try JSONDecoder().decode(Snapshot.self, from: json)
        self.init(
            inGame: snapshot.inGame, gold: snapshot.gold, currencyUnit: snapshot.currencyUnit ?? "",
            party: (snapshot.party ?? []).map(\.member), items: (snapshot.items ?? []).compactMap(\.item),
            variables: (snapshot.variables ?? []).map { CheatVariable(id: $0.id, name: $0.name, value: $0.value.value) },
            switches: (snapshot.switches ?? []).map { CheatSwitch(id: $0.id, name: $0.name, isOn: $0.on) })
    }
}

/// The state's JSON. Lists come as arrays, which keep thousands of variables short.
private struct Snapshot: Decodable {
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
