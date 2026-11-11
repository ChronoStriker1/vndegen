import Foundation
import VNCore

nonisolated enum ArtemisControls {
    static let version = 1

    static let roles: [(role: String, label: String)] = [
        ("AUTO", "Auto"), ("QSAVE", "Quick save"), ("QLOAD", "Quick load"), ("CONFIG", "Config"), ("SAVE", "Save"),
        ("LOAD", "Load"), ("VOICE", "Replay voice"),
    ]
    static let sent: Set<Int> = [1, 2, 13, 17, 27, 37, 38, 39, 40, 136, 137]
    static let replaceable: [(input: ControlInput, key: Int)] = [(.view, 9), (.l3, 16), (.r3, 1), (.l2, 2)]

    struct KeyBinding: Equatable {
        var code: Int
        var role: String
    }

    static func suggestion(_ files: ArtemisCore.Files) -> ControlSuggestion? {
        files.read("system/table/list_windows.tbl").flatMap { suggestion(listWindows: String(decoding: $0, as: UTF8.self)) }
    }

    static func suggestion(listWindows: String) -> ControlSuggestion? {
        suggestion(advKeys(listWindows))
    }

    static func suggestion(_ bindings: [KeyBinding]) -> ControlSuggestion? {
        let bound = Set(bindings.map(\.code))
        let reached = Set(bindings.filter { sent.contains($0.code) }.map(\.role))
        let free = replaceable.filter { !bound.contains($0.key) || sent.contains($0.key) }.map(\.input)
        let placing = roles.filter { !reached.contains($0.role) }.compactMap { role, label -> ControlSuggestion.Binding? in
            guard let key = bindings.lazy.filter({ $0.role == role }).compactMap({ KeyboardKey(windowsKeyCode: $0.code) }).first
            else { return nil }
            return .init(.key(key), label: label)
        }
        return ControlSuggestion(placing: placing, on: free)
    }

    static func advKeys(_ text: String) -> [KeyBinding] {
        guard let start = text.firstMatch(of: /advkey\s*=\s*\{\s*def\s*=\s*\{/)?.range.upperBound else { return [] }
        let rest = text[start...]
        let table = rest[..<(rest.firstMatch(of: /\}\s*,\s*tbl\s*=/)?.range.lowerBound ?? rest.endIndex)]
        return table.matches(of: /\[(\d+)\]\s*=\s*\{([^}]*)\}/).compactMap { match in
            guard let code = Int(match.1), code < 256, let role = match.2.firstMatch(of: /adv\s*=\s*"(\w+)"/)?.1 else { return nil }
            return KeyBinding(code: code, role: String(role))
        }
    }
}
