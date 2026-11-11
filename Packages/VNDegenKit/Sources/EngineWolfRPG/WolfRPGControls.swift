import CWolfRPG
import Foundation
import VNCore

nonisolated enum WolfRPGControls {
    static let version = 1

    static let free: [ControlInput] = [.view, .l2, .l3, .r3]
    static let sent: Set<Int> = [129, 142, 144, 145, 300, 303, 305, 308]
    static let names: [String: String] = [
        "バックログ表示": "Backlog", "【並】バックログ表示": "Backlog", "ログ表示": "Backlog", "メッセージ簡易ログ": "Message log",
        "メッセージウィンドウ消去": "Hide window", "戦闘高速化": "Fast battle", "メッセージスキップ": "Skip",
        "【並】メッセージスキップ": "Skip", "文章スキップ": "Skip",
    ]

    struct KeyCheck: Equatable {
        var code: Int
        var event: String
    }

    static func suggestion(root: URL) -> ControlSuggestion? {
        guard let text = vnwolf_key_checks(root.path) else { return nil }
        return suggestion(checks(String(cString: text)))
    }

    static func checks(_ text: String) -> [KeyCheck] {
        text.split(separator: "\n").compactMap { line in
            let fields = line.split(separator: "\t", maxSplits: 1, omittingEmptySubsequences: false)
            return Int(fields[0]).map { KeyCheck(code: $0, event: fields.count > 1 ? String(fields[1]) : "") }
        }
    }

    static func suggestion(_ checks: [KeyCheck]) -> ControlSuggestion? {
        var order: [Int] = []
        var labels: [Int: String] = [:]
        for check in checks where !sent.contains(check.code) && !isDebug(check.event) && key(wolfCode: check.code) != nil {
            if !order.contains(check.code) { order.append(check.code) }
            if labels[check.code] == nil, let label = names[check.event] { labels[check.code] = label }
        }
        let named = order.filter { labels[$0] != nil } + order.filter { labels[$0] == nil }
        let bindings = named.compactMap { code in key(wolfCode: code).map { ControlSuggestion.Binding(.key($0), label: labels[code]) } }
        return ControlSuggestion(placing: bindings, on: free)
    }

    static func isDebug(_ event: String) -> Bool {
        event.range(of: "debug|デバッグ|ﾃﾞﾊﾞｯｸﾞ|cheat|チート|ﾁｰﾄ", options: [.regularExpression, .caseInsensitive]) != nil
    }

    static func key(wolfCode code: Int) -> KeyboardKey? {
        let rows: [(start: Int, letters: String)] = [(116, "QWERTYUIOP"), (130, "ASDFGHJKL"), (144, "ZXCVBNM")]
        for row in rows where (row.start..<row.start + row.letters.count).contains(code) {
            let letter = row.letters.unicodeScalars[row.letters.unicodeScalars.index(row.letters.unicodeScalars.startIndex, offsetBy: code - row.start)]
            return KeyboardKey(windowsKeyCode: Int(letter.value))
        }
        let named: [Int: Int] = [101: 27, 111: 48, 114: 8, 115: 9, 128: 13, 157: 32]
        if (102...110).contains(code) { return KeyboardKey(windowsKeyCode: 49 + code - 102) }
        return named[code].flatMap(KeyboardKey.init(windowsKeyCode:))
    }
}
