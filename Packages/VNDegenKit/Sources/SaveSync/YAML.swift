import Foundation

/// The YAML subset Ludusavi reads and writes through serde_yaml 0.8: block mappings and sequences, plain and quoted
/// scalars, `~`, and empty flow collections. Ludusavi's own mapping.yaml and config.yaml use nothing else.
indirect enum YAML: Equatable, Sendable {
    case null
    case scalar(String)
    /// A number or boolean, written without quotes. The reader returns these as `scalar`.
    case literal(String)
    case mapping([Entry])
    case sequence([YAML])

    struct Entry: Equatable, Sendable {
        var key: String
        var value: YAML
    }

    subscript(key: String) -> YAML? {
        guard case let .mapping(entries) = self else { return nil }
        return entries.first { $0.key == key }?.value
    }

    var string: String? {
        switch self {
        case let .scalar(value), let .literal(value): value
        default: nil
        }
    }

    var entries: [Entry] {
        if case let .mapping(entries) = self { return entries }
        return []
    }

    var items: [YAML] {
        if case let .sequence(items) = self { return items }
        return []
    }
}

struct YAMLError: Error, CustomStringConvertible {
    let line: Int
    let reason: String
    var description: String { "mapping.yaml line \(line): \(reason)" }
}

// MARK: - Reading

extension YAML {
    static func parse(_ text: String) throws -> YAML {
        var parser = Parser(text)
        return try parser.document()
    }

    private struct Line {
        var number: Int
        var indent: Int
        var text: Substring
    }

    private struct Parser {
        var lines: [Line]
        var index = 0

        init(_ text: String) {
            lines = text.split(separator: "\n", omittingEmptySubsequences: false).enumerated().compactMap { offset, raw in
                let raw = raw.hasSuffix("\r") ? raw.dropLast() : raw
                let body = raw.drop { $0 == " " }
                if body.isEmpty || body.hasPrefix("#") || body == "---" || body == "..." { return nil }
                return Line(number: offset + 1, indent: raw.count - body.count, text: body)
            }
        }

        mutating func document() throws -> YAML {
            if let first = lines.first, first.text.hasPrefix("--- ") {
                // A document whose root is a scalar or flow collection, such as `--- {}`.
                lines.removeFirst()
                return try scalar(first.text.dropFirst(4), line: first.number)
            }
            guard index < lines.count else { return .null }
            let node = try block(indent: lines[index].indent)
            if index < lines.count { throw YAMLError(line: lines[index].number, reason: "unexpected indentation") }
            return node
        }

        private func isItem(_ text: Substring) -> Bool { text == "-" || text.hasPrefix("- ") }

        private mutating func block(indent: Int) throws -> YAML {
            isItem(lines[index].text) ? try sequence(indent: indent) : try mapping(indent: indent)
        }

        /// The value after `key:` or `-` with nothing else on the line: a nested block, or null.
        private mutating func nested(under indent: Int, allowSameIndentSequence: Bool) throws -> YAML {
            guard index < lines.count else { return .null }
            let next = lines[index]
            if next.indent > indent || (allowSameIndentSequence && next.indent == indent && isItem(next.text)) {
                return try block(indent: next.indent)
            }
            return .null
        }

        private mutating func mapping(indent: Int) throws -> YAML {
            var entries: [Entry] = []
            while index < lines.count, lines[index].indent == indent, !isItem(lines[index].text) {
                let line = lines[index]
                guard let (key, rest) = try Self.splitKey(line.text, line: line.number) else {
                    throw YAMLError(line: line.number, reason: "expected a key")
                }
                index += 1
                let value = rest.isEmpty
                    ? try nested(under: indent, allowSameIndentSequence: true)
                    : try scalar(rest, line: line.number)
                entries.append(Entry(key: key, value: value))
            }
            return .mapping(entries)
        }

        private mutating func sequence(indent: Int) throws -> YAML {
            var items: [YAML] = []
            while index < lines.count, lines[index].indent == indent, isItem(lines[index].text) {
                let line = lines[index]
                let rest = line.text.dropFirst().drop { $0 == " " }
                if rest.isEmpty {
                    index += 1
                    items.append(try nested(under: indent, allowSameIndentSequence: false))
                } else if try isItem(rest) || Self.splitKey(rest, line: line.number) != nil {
                    // `- key: value` or `- - item`: the rest of the line starts a block at its own column.
                    let column = indent + line.text.count - rest.count
                    lines[index] = Line(number: line.number, indent: column, text: rest)
                    items.append(try block(indent: column))
                } else {
                    index += 1
                    items.append(try scalar(rest, line: line.number))
                }
            }
            return .sequence(items)
        }

        /// Splits `key: value` or `key:`. Returns nil when the text is a scalar rather than a mapping entry.
        static func splitKey(_ text: Substring, line: Int) throws -> (String, Substring)? {
            let key: String
            var rest: Substring
            if let quote = text.first, quote == "\"" || quote == "'" {
                guard let (parsed, after) = try quoted(text, line: line) else { return nil }
                key = parsed
                rest = after
                guard rest.first == ":" else { return nil }
                rest = rest.dropFirst()
            } else {
                var cut: Substring.Index?
                var i = text.startIndex
                while i < text.endIndex {
                    if text[i] == ":" {
                        let next = text.index(after: i)
                        if next == text.endIndex || text[next] == " " { cut = i; break }
                    }
                    if text[i] == " ", text.index(after: i) < text.endIndex, text[text.index(after: i)] == "#" { break }
                    i = text.index(after: i)
                }
                guard let cut else { return nil }
                key = String(text[..<cut])
                rest = text[text.index(after: cut)...]
            }
            guard rest.isEmpty || rest.first == " " else { return nil }
            rest = rest.drop { $0 == " " }
            if rest.hasPrefix("#") { rest = rest.prefix(0) }
            return (key, rest)
        }

        func scalar(_ raw: Substring, line: Int) throws -> YAML {
            if raw.first == "\"" || raw.first == "'" {
                guard let (value, after) = try Self.quoted(raw, line: line) else {
                    throw YAMLError(line: line, reason: "unterminated string")
                }
                let trailing = after.drop { $0 == " " }
                guard trailing.isEmpty || trailing.hasPrefix("#") else {
                    throw YAMLError(line: line, reason: "text after a quoted string")
                }
                return .scalar(value)
            }
            var text = raw
            if let comment = text.range(of: " #") { text = text[..<comment.lowerBound] }
            while text.last == " " { text = text.dropLast() }
            switch text {
            case "~", "null", "Null", "NULL": return .null
            case "[]": return .sequence([])
            case "{}": return .mapping([])
            default:
                if let first = text.first, "[{|>&*!".contains(first) {
                    throw YAMLError(line: line, reason: "unsupported YAML syntax \(text)")
                }
                return .scalar(String(text))
            }
        }

        /// Parses a quoted string at the start of `text`. Returns the value and the text after the closing quote.
        static func quoted(_ text: Substring, line: Int) throws -> (String, Substring)? {
            guard let quote = text.first else { return nil }
            var value = ""
            var i = text.index(after: text.startIndex)
            while i < text.endIndex {
                let c = text[i]
                if quote == "'" {
                    if c == "'" {
                        let next = text.index(after: i)
                        if next < text.endIndex, text[next] == "'" { value.append("'"); i = text.index(after: next); continue }
                        return (value, text[next...])
                    }
                    value.append(c)
                    i = text.index(after: i)
                    continue
                }
                if c == "\"" { return (value, text[text.index(after: i)...]) }
                guard c == "\\" else { value.append(c); i = text.index(after: i); continue }
                i = text.index(after: i)
                guard i < text.endIndex else { break }
                let escape = text[i]
                i = text.index(after: i)
                let hexLength: Int? = switch escape {
                case "x": 2
                case "u": 4
                case "U": 8
                default: nil
                }
                if let hexLength {
                    guard let end = text.index(i, offsetBy: hexLength, limitedBy: text.endIndex),
                          let code = UInt32(text[i..<end], radix: 16), let scalar = Unicode.Scalar(code) else {
                        throw YAMLError(line: line, reason: "bad escape")
                    }
                    value.unicodeScalars.append(scalar)
                    i = end
                    continue
                }
                switch escape {
                case "0": value.append("\0")
                case "a": value.append("\u{07}")
                case "b": value.append("\u{08}")
                case "t": value.append("\t")
                case "n": value.append("\n")
                case "v": value.append("\u{0B}")
                case "f": value.append("\u{0C}")
                case "r": value.append("\r")
                case "e": value.append("\u{1B}")
                case "N": value.append("\u{85}")
                case "_": value.append("\u{A0}")
                case "L": value.append("\u{2028}")
                case "P": value.append("\u{2029}")
                case "\"", "\\", "/", " ": value.append(escape)
                default: throw YAMLError(line: line, reason: "bad escape \\\(escape)")
                }
            }
            return nil
        }
    }
}

// MARK: - Writing

extension YAML {
    /// Formats like serde_yaml 0.8, so files Ludusavi rewrites stay byte-identical where nothing changed.
    func serialized() -> String {
        var out = "---"
        switch self {
        case let .mapping(entries) where isBlock: Self.write(entries, indent: 0, into: &out)
        case let .sequence(items) where isBlock: Self.write(items, indent: 0, into: &out)
        default: out += " " + inline
        }
        return out + "\n"
    }

    /// The text after `key: ` or `- ` for a value that is not a non-empty collection.
    private var inline: String {
        switch self {
        case .null: "~"
        case let .scalar(value): Self.quote(value)
        case let .literal(value): value
        case .mapping: "{}"
        case .sequence: "[]"
        }
    }

    private var isBlock: Bool {
        switch self {
        case let .mapping(entries): !entries.isEmpty
        case let .sequence(items): !items.isEmpty
        default: false
        }
    }

    private static func write(_ entries: [Entry], indent: Int, into out: inout String) {
        let pad = String(repeating: " ", count: indent)
        for entry in entries {
            out += "\n\(pad)\(quote(entry.key)):"
            switch entry.value {
            case let .mapping(nested) where !nested.isEmpty: write(nested, indent: indent + 2, into: &out)
            case let .sequence(items) where !items.isEmpty: write(items, indent: indent + 2, into: &out)
            case let value: out += " " + value.inline
            }
        }
    }

    private static func write(_ items: [YAML], indent: Int, into out: inout String) {
        let pad = String(repeating: " ", count: indent)
        for item in items {
            guard item.isBlock else {
                out += "\n\(pad)- \(item.inline)"
                continue
            }
            // Write the nested block two columns deeper, then fold its first line onto the dash.
            var nested = ""
            switch item {
            case let .mapping(entries): write(entries, indent: indent + 2, into: &nested)
            case let .sequence(items): write(items, indent: indent + 2, into: &nested)
            default: break
            }
            out += "\n\(pad)- " + nested.dropFirst(indent + 3)
        }
    }

    /// yaml-rust's `need_quotes` rules, which serde_yaml 0.8 uses.
    static func quote(_ value: String) -> String {
        guard needsQuotes(value) else { return value }
        var out = "\""
        for scalar in value.unicodeScalars {
            switch scalar {
            case "\"": out += "\\\""
            case "\\": out += "\\\\"
            case "\u{08}": out += "\\b"
            case "\t": out += "\\t"
            case "\n": out += "\\n"
            case "\u{0C}": out += "\\f"
            case "\r": out += "\\r"
            case "\u{00}"..."\u{1F}", "\u{7F}": out += String(format: "\\u%04x", scalar.value)
            default: out.unicodeScalars.append(scalar)
            }
        }
        return out + "\""
    }

    private static let reservedWords: Set<String> = [
        "y", "Y", "yes", "Yes", "YES", "n", "N", "no", "No", "NO", "true", "True", "TRUE", "false", "False", "FALSE",
        "on", "On", "ON", "off", "Off", "OFF", "null", "Null", "NULL", "~",
    ]

    private static func needsQuotes(_ value: String) -> Bool {
        guard let first = value.first, let last = value.last else { return true }
        if first == " " || last == " " || first == "." || value.hasPrefix("0x") { return true }
        if "&*?|-<>=!%@".contains(first) { return true }
        if value.unicodeScalars.contains(where: { ":{}[],#`\"'\\\t\n\r".unicodeScalars.contains($0) || $0.value < 0x20 }) {
            return true
        }
        return reservedWords.contains(value) || Int64(value) != nil || Double(value) != nil
    }
}
