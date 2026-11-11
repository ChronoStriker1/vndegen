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
        let text = { (path: String) in files.read(path).map { String(decoding: $0, as: UTF8.self) } }
        return suggestion(listWindows: text("system/table/list_windows.tbl"), variables: text("system/adv/var.lua"))
    }

    static func suggestion(listWindows: String?, variables: String?) -> ControlSuggestion? {
        let bindings = listWindows.map(advKeys).flatMap { $0.isEmpty ? nil : $0 } ?? variables.map(keyConfig) ?? []
        return suggestion(bindings)
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

    static func keyConfig(_ text: String) -> [KeyBinding] {
        let code = text.split(whereSeparator: \.isNewline).map { line -> Substring in
            line.firstRange(of: "--").map { line[..<$0.lowerBound] } ?? line
        }.joined(separator: "\n")
        func table(from start: String.Index?) -> Substring? {
            guard let start else { return nil }
            let rest = code[start...]
            return rest[..<(rest.firstMatch(of: /\n\}/)?.range.lowerBound ?? rest.endIndex)]
        }
        let fixed = table(from: code.firstMatch(of: /keyconfig_keytable\s*=\s*\{/)?.range.upperBound).map { body in
            body.matches(of: /(\w+)\s*=\s*\{\s*\d+\s*,\s*(\d+)\s*\}/).compactMap { match in
                Int(match.2).map { KeyBinding(code: $0, role: String(match.1)) }
            }
        } ?? []
        let shortcuts = table(from: code.firstMatch(of: /keyconfig_shortcut_table\s*=\s*\{/)?.range.upperBound).map { body in
            body.matches(of: /\[(\d+)\]\s*=\s*"(\w+)"/).compactMap { match in
                Int(match.1).map { KeyBinding(code: $0, role: String(match.2)) }
            }
        } ?? []
        return fixed + shortcuts
    }
}
