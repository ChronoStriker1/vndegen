import Foundation

/// A running game's cheats, for VNDegen's Cheats page. Every change goes through the engine's own state, so a save made
/// afterwards is an ordinary save in the PC format. A session starts with the settings in `GameLaunch.game.cheats`.
@MainActor
public protocol CheatSession: AnyObject {
    /// What this game can do. The page shows nothing else.
    var features: CheatFeatures { get }
    /// The game's state now.
    func state() async throws -> CheatState
    func perform(_ action: CheatAction) async throws
    /// The cheats that stay on, each time the player changes one.
    func apply(_ settings: CheatSettings)
}

public struct CheatFeatures: OptionSet, Hashable, Sendable {
    public let rawValue: Int
    public init(rawValue: Int) { self.rawValue = rawValue }

    public static let gold = CheatFeatures(rawValue: 1 << 0)
    public static let restoreParty = CheatFeatures(rawValue: 1 << 1)
    public static let items = CheatFeatures(rawValue: 1 << 2)
    public static let keepPartyHealthy = CheatFeatures(rawValue: 1 << 3)
    public static let noEncounters = CheatFeatures(rawValue: 1 << 4)
    public static let walkThroughWalls = CheatFeatures(rawValue: 1 << 5)
    public static let speed = CheatFeatures(rawValue: 1 << 6)
    public static let experience = CheatFeatures(rawValue: 1 << 7)
    public static let variables = CheatFeatures(rawValue: 1 << 8)
    public static let switches = CheatFeatures(rawValue: 1 << 9)
}

/// The cheats that stay on until the player turns them off. The library keeps them per game, and the game applies them
/// each time it starts. All are off by default.
public struct CheatSettings: Codable, Hashable, Sendable {
    /// The party's HP and MP don't go down.
    public var keepsPartyHealthy = false
    public var noEncounters = false
    public var walksThroughWalls = false
    /// Game updates per displayed frame, one of `speeds`. The game's own timers run faster with it, so they stay in step.
    public var speed = 1
    /// What experience the party gains is multiplied by, one of `experienceRates`.
    public var experienceRate = 1

    public static let speeds = [1, 2, 4]
    public static let experienceRates = [1, 2, 5, 10]

    public init() {}

    /// A key missing from the library, as from an older version, keeps its default.
    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        keepsPartyHealthy = try container.decodeIfPresent(Bool.self, forKey: .keepsPartyHealthy) ?? false
        noEncounters = try container.decodeIfPresent(Bool.self, forKey: .noEncounters) ?? false
        walksThroughWalls = try container.decodeIfPresent(Bool.self, forKey: .walksThroughWalls) ?? false
        speed = try container.decodeIfPresent(Int.self, forKey: .speed) ?? 1
        experienceRate = try container.decodeIfPresent(Int.self, forKey: .experienceRate) ?? 1
    }

    public var isActive: Bool { self != CheatSettings() }

    /// What is on, for the game page, such as "2x speed".
    public var summary: [String] {
        var on: [String] = []
        if speed != 1 { on.append("\(speed)x speed") }
        if keepsPartyHealthy { on.append("HP and MP kept full") }
        if noEncounters { on.append("no random encounters") }
        if walksThroughWalls { on.append("walk through walls") }
        if experienceRate != 1 { on.append("\(experienceRate)x experience") }
        return on
    }
}

/// What the Cheats page shows of a running game.
public struct CheatState: Sendable, Equatable {
    /// False before the player starts or loads a game, such as on the title screen, where starting or loading a game
    /// would replace any change.
    public var inGame: Bool
    public var gold: Int?
    /// Such as "G".
    public var currencyUnit: String
    public var party: [CheatPartyMember]
    public var items: [CheatItem]
    public var variables: [CheatVariable]
    public var switches: [CheatSwitch]

    public init(inGame: Bool, gold: Int? = nil, currencyUnit: String = "", party: [CheatPartyMember] = [],
                items: [CheatItem] = [], variables: [CheatVariable] = [], switches: [CheatSwitch] = []) {
        self.inGame = inGame
        self.gold = gold
        self.currencyUnit = currencyUnit
        self.party = party
        self.items = items
        self.variables = variables
        self.switches = switches
    }
}

public struct CheatPartyMember: Sendable, Hashable {
    public var name: String
    public var hp: Int
    public var maxHP: Int
    public var mp: Int
    public var maxMP: Int

    public init(name: String, hp: Int, maxHP: Int, mp: Int, maxMP: Int) {
        self.name = name
        self.hp = hp
        self.maxHP = maxHP
        self.mp = mp
        self.maxMP = maxMP
    }
}

/// An entry of the game's item, weapon or armor database.
public struct CheatItem: Identifiable, Sendable, Hashable {
    public enum Kind: String, CaseIterable, Sendable {
        case item, weapon, armor

        public var title: String {
            switch self {
            case .item: "Items"
            case .weapon: "Weapons"
            case .armor: "Armor"
            }
        }
    }

    public struct ID: Hashable, Sendable {
        public let kind: Kind
        public let number: Int

        public init(kind: Kind, number: Int) {
            self.kind = kind
            self.number = number
        }
    }

    public let id: ID
    public let name: String
    /// How many the party has.
    public let owned: Int

    public init(kind: Kind, number: Int, name: String, owned: Int) {
        self.id = ID(kind: kind, number: number)
        self.name = name
        self.owned = owned
    }
}

/// One of the game's numbered variables, with the name its database gives it.
public struct CheatVariable: Identifiable, Sendable, Hashable {
    public let id: Int
    public let name: String
    public let value: CheatValue

    public init(id: Int, name: String, value: CheatValue) {
        self.id = id
        self.name = name
        self.value = value
    }
}

public struct CheatSwitch: Identifiable, Sendable, Hashable {
    public let id: Int
    public let name: String
    public let isOn: Bool

    public init(id: Int, name: String, isOn: Bool) {
        self.id = id
        self.name = name
        self.isOn = isOn
    }
}

public enum CheatValue: Sendable, Hashable {
    case integer(Int)
    case text(String)
    /// A value the page shows but doesn't edit, such as a list a script keeps in a variable, as the engine describes it.
    case other(String)
}

public enum CheatAction: Sendable, Hashable {
    case addGold(Int)
    /// Full HP and MP, and no states, for the whole party.
    case restoreParty
    case addItem(CheatItem.ID, count: Int)
    case setVariable(id: Int, CheatValue)
    case setSwitch(id: Int, Bool)
}
