import Foundation

/// The `.reg` files regedit reads and writes, version 5 and REGEDIT4. Writing follows regashii, which Ludusavi writes its
/// `registry.reg` with, byte for byte, because Ludusavi's recorded hash is the SHA-1 of that text. Reading follows
/// regashii's grammar for the forms regedit, Wine's regedit and regashii write.
extension RegistryData {
    public enum ReadError: Error, Equatable {
        case encoding
        case header
    }

    /// Reads a `.reg` file in UTF-8, or in UTF-16 with a byte order mark, as regedit 5 writes it.
    public init(reg data: Data) throws {
        guard let text = Self.decode(data) else { throw ReadError.encoding }
        try self.init(reg: text)
    }

    public init(reg text: String) throws {
        self.init()
        var scanner = RegScanner(text.hasPrefix("\u{FEFF}") ? String(text.dropFirst()) : text)
        guard let format = scanner.header() else { throw ReadError.header }
        var current: String?
        var pending: [String: RegistryValue] = [:]
        func flush() {
            if let current { add(pending, to: current) }
            pending = [:]
        }
        while !scanner.atEnd {
            switch scanner.element(format) {
            case let .key(name, deleted):
                flush()
                if deleted {
                    // A deleted key is left out, and one added again after it holds only what follows, as regashii
                    // replaces it.
                    removeKey(name)
                    current = nil
                } else {
                    current = name
                }
            case let .value(name, value):
                guard current != nil else { continue }
                let lower = name.lowercased()
                let existing = pending.keys.first { $0.lowercased() == lower }
                if let value { pending[existing ?? name] = value } else if let existing { pending[existing] = nil }
            case .skip:
                continue
            }
        }
        flush()
    }

    /// The text Ludusavi writes as `registry.reg`, which its hash covers. Like regashii, it writes every name as it is,
    /// even one holding a line break, which regedit has no way to write. A file that goes to Wine's regedit must hold
    /// only `writable` values.
    public var regText: String {
        var parts = [Self.regedit5Header]
        for (key, values) in keys.sortedByUTF8 {
            parts.append("\n\n")
            var lines = ["[\(key)]"]
            for (name, value) in values.sortedByUTF8 {
                let quoted = "\"\(Self.escape(name))\""
                lines.append("\(quoted)=\(Self.serialize(value, offset: quoted.utf8.count + 1))")
            }
            parts.append(lines.joined(separator: "\n"))
        }
        parts.append("\n")
        return parts.joined()
    }

    /// regedit 5's own encoding, UTF-16 little-endian with a byte order mark, which Wine's regedit needs for names and
    /// strings outside ASCII.
    public static func utf16File(_ text: String) -> Data {
        var data = Data([0xFF, 0xFE])
        for unit in text.utf16 { data.append(contentsOf: [UInt8(unit & 0xFF), UInt8(unit >> 8)]) }
        return data
    }

    static func decode(_ data: Data) -> String? {
        let bytes = [UInt8](data)
        if bytes.starts(with: [0xFF, 0xFE]) { return String(data: data.dropFirst(2), encoding: .utf16LittleEndian) }
        if bytes.starts(with: [0xFE, 0xFF]) { return String(data: data.dropFirst(2), encoding: .utf16BigEndian) }
        return String(data: data, encoding: .utf8)
    }

    /// True when `text` holds a line break, which a `.reg` name can't, so written as is it would start a line, and so
    /// a section, of its own.
    public static func breaksLine(_ text: String) -> Bool {
        text.unicodeScalars.contains { $0 == "\n" || $0 == "\r" }
    }

    /// `values` without the ones whose names a `.reg` file can't hold.
    public static func writable(_ values: [String: RegistryValue]) -> [String: RegistryValue] {
        values.filter { !breaksLine($0.key) }
    }

    public static func escape(_ text: String) -> String {
        text.replacingOccurrences(of: "\\", with: "\\\\").replacingOccurrences(of: "\"", with: "\\\"")
    }

    /// How regedit 5 writes a value, regashii's `Value::into_raw`.
    enum Encoded: Equatable {
        case quoted(String)
        case dword(UInt32)
        case hex(kind: UInt8, bytes: [UInt8])
    }

    static func encoded(_ value: RegistryValue) -> Encoded {
        switch value {
        case let .sz(text):
            if text.contains(where: { $0 == "\n" || $0 == "\r" || $0 == "\r\n" }) { return .hex(kind: 1, bytes: utf16z(text)) }
            return .quoted(text)
        case let .expandSz(text): return .hex(kind: 2, bytes: utf16z(text))
        case let .multiSz(joined):
            var bytes: [UInt8] = []
            for part in joined.components(separatedBy: "\n") where !part.isEmpty { bytes += utf16z(part) }
            return .hex(kind: 7, bytes: bytes + [0, 0])
        case let .dword(number): return .dword(number)
        case let .qword(number): return .hex(kind: 0xb, bytes: withUnsafeBytes(of: number.littleEndian, Array.init))
        case let .binary(bytes): return .hex(kind: 3, bytes: bytes)
        case let .raw(kind, bytes): return .hex(kind: kind, bytes: bytes)
        }
    }

    /// regashii's `serialize::value`. `offset` is how far into its line the value starts.
    static func serialize(_ value: RegistryValue, offset: Int) -> String {
        switch encoded(value) {
        case let .quoted(text): "\"\(escape(text))\""
        case let .dword(number): "dword:" + String(format: "%08x", number)
        case let .hex(kind, bytes): hex(kind, bytes, offset: offset)
        }
    }

    /// UTF-16 little-endian without trailing nulls, then one null, as regashii's `str_to_utf16_bytes`.
    static func utf16z(_ text: String) -> [UInt8] {
        var trimmed = Substring(text)
        while trimmed.last == "\0" { trimmed = trimmed.dropLast() }
        return trimmed.utf16.flatMap { [UInt8($0 & 0xFF), UInt8($0 >> 8)] } + [0, 0]
    }

    /// `hex:` for binary, `hex(<kind>):` otherwise, wrapped after 76 columns as regedit does.
    static func hex(_ kind: UInt8, _ bytes: [UInt8], offset: Int) -> String {
        var out = kind == 3 ? "hex:" : "hex(\(String(kind, radix: 16))):"
        var running = offset + out.utf8.count
        for (index, byte) in bytes.enumerated() {
            out += String(format: "%02x", byte)
            running += 2
            guard index + 1 < bytes.count else { continue }
            out += ","
            running += 1
            if running > 76 {
                out += "\\\n  "
                running = 2
            }
        }
        return out
    }
}

private enum RegFormat { case regedit5, regedit4 }

private enum RegElement {
    case key(String, deleted: Bool)
    /// A nil value deletes the value.
    case value(String, RegistryValue?)
    case skip
}

/// Walks a `.reg` file's text. Lines end with LF, CRLF or CR. Anything it can't read is skipped to the end of its line.
private struct RegScanner {
    private let chars: [Character]
    private var at = 0

    init(_ text: String) {
        // A CRLF is one Character in Swift, so each line end is one character here.
        chars = Array(text)
    }

    var atEnd: Bool { at >= chars.count }

    private var current: Character? { at < chars.count ? chars[at] : nil }
    private static func isLineEnd(_ char: Character) -> Bool { char == "\n" || char == "\r" || char == "\r\n" }

    mutating func header() -> RegFormat? {
        for (text, format) in [(RegistryData.regedit4Header, RegFormat.regedit4), (RegistryData.regedit5Header, .regedit5)] {
            let header = Array(text)
            guard chars.count > header.count, Array(chars[0..<header.count]) == header else { continue }
            let tail = chars[header.count]
            guard tail == " " || tail == "\t" || tail == ";" || Self.isLineEnd(tail) else { continue }
            at = header.count
            return format
        }
        return nil
    }

    private mutating func skipSpaces() {
        while let char = current, char == " " || char == "\t" { at += 1 }
    }

    private mutating func skipLine() -> RegElement {
        while let char = current, !Self.isLineEnd(char) { at += 1 }
        return .skip
    }

    mutating func element(_ format: RegFormat) -> RegElement {
        guard let char = current else { return .skip }
        if Self.isLineEnd(char) {
            at += 1
            return .skip
        }
        switch char {
        case "[": return key()
        case ";": return skipLine()
        case "@", "\"", " ", "\t": return value(format)
        default: return skipLine()
        }
    }

    private mutating func restOfLine() -> String {
        let start = at
        while let char = current, !Self.isLineEnd(char) { at += 1 }
        return String(chars[start..<at])
    }

    private mutating func key() -> RegElement {
        at += 1
        if current == " " || current == "]" { return skipLine() }
        let deleted = current == "-"
        if deleted { at += 1 }
        if current == " " || current == "]" { return skipLine() }
        let line = restOfLine()
        guard let close = line.lastIndex(of: "]") else { return .skip }
        return .key(RegistryData.normalized(String(line[..<close])), deleted: deleted)
    }

    private static let escapes: [Character: Character] = ["\\": "\\", "\"": "\"", "n": "\n", "r": "\r", "0": "\0"]

    private mutating func quoted() -> String? {
        guard current == "\"" else { return nil }
        at += 1
        var text = ""
        while let char = current {
            at += 1
            switch char {
            case "\"": return text
            case "\\":
                // Wine's regedit also writes \n, \r and \0 in strings, which Windows' regedit writes as hex(1).
                guard let next = current, let unescaped = Self.escapes[next] else { return nil }
                text.append(unescaped)
                at += 1
            default:
                // regashii takes a line break inside quotes as part of the text, and writes one back that way, so a
                // name holding one in a backup's registry survives a rewrite.
                text.append(char)
            }
        }
        return nil
    }

    private mutating func value(_ format: RegFormat) -> RegElement {
        skipSpaces()
        let name: String
        if current == "@" {
            at += 1
            // Ludusavi keeps the default value under an empty name and writes it back as `""`.
            name = ""
        } else if let read = quoted() {
            name = read
        } else {
            return skipLine()
        }
        skipSpaces()
        guard current == "=" else { return skipLine() }
        at += 1
        skipSpaces()
        guard let raw = data() else { return skipLine() }
        skipSpaces()
        if current == ";" { _ = skipLine() }
        // regedit won't import a value with anything else after it on its line.
        if let char = current, !Self.isLineEnd(char) { return skipLine() }
        return .value(name, raw.map { Self.convert($0, format) } ?? nil)
    }

    private enum Raw {
        case sz(String)
        case dword(UInt32)
        case hex(UInt8, [UInt8])
    }

    /// The value after `=`. The outer nil means unreadable, the inner nil a deleted value.
    private mutating func data() -> Raw?? {
        if current == "-" {
            at += 1
            return .some(nil)
        }
        if current == "\"" { return quoted().map { .some(.sz($0)) } }
        if take("dword:") {
            let start = at
            while let char = current, char.isHexDigit, at - start < 8 { at += 1 }
            guard at > start, let number = UInt32(String(chars[start..<at]), radix: 16) else { return nil }
            return .some(.dword(number))
        }
        guard take("hex") else { return nil }
        var kind: UInt8 = 3
        if take("(") {
            let start = at
            while let char = current, char.isHexDigit { at += 1 }
            guard at > start, let read = UInt8(String(chars[start..<at]), radix: 16), take(")") else { return nil }
            kind = read
        }
        guard take(":") else { return nil }
        var bytes: [UInt8] = []
        while true {
            skipSpaces()
            guard let high = current, high.isHexDigit else { break }
            let start = at
            while let char = current, char.isHexDigit, at - start < 2 { at += 1 }
            guard let byte = UInt8(String(chars[start..<at]), radix: 16) else { return nil }
            bytes.append(byte)
            skipSpaces()
            guard take(",") else { break }
            skipSpaces()
            // A line ending in a backslash goes on with the next, indented.
            if take("\\") {
                skipSpaces()
                guard let end = current, Self.isLineEnd(end) else { return nil }
                at += 1
            }
        }
        return .some(.hex(kind, bytes))
    }

    private mutating func take(_ text: String) -> Bool {
        let wanted = Array(text)
        guard at + wanted.count <= chars.count, Array(chars[at..<(at + wanted.count)]) == wanted else { return false }
        at += wanted.count
        return true
    }

    /// regashii's `Value::from_raw`, then Ludusavi's conversion to its own entries. A hex value whose typed form would
    /// be written back otherwise, such as `hex(4)` for a DWORD, stays raw, so a file VNDegen rewrites keeps every value
    /// it didn't change as it was. Ludusavi reads both forms as the same value.
    private static func convert(_ raw: Raw, _ format: RegFormat) -> RegistryValue? {
        guard case let .hex(kind, bytes) = raw else { return typed(raw, format) }
        guard let value = typed(raw, format) else { return nil }
        return RegistryData.encoded(value) == .hex(kind: kind, bytes: bytes) ? value : .raw(kind: kind, bytes: bytes)
    }

    private static func typed(_ raw: Raw, _ format: RegFormat) -> RegistryValue? {
        switch raw {
        case let .sz(text): return .sz(text)
        case let .dword(number): return .dword(number)
        case let .hex(kind, bytes):
            let text = { format == .regedit5 ? utf16String(bytes) : String(bytes: bytes, encoding: .utf8).map(trimNulls) }
            switch kind {
            case 1: return text().map(RegistryValue.sz) ?? .raw(kind: kind, bytes: bytes)
            case 2: return text().map(RegistryValue.expandSz) ?? .raw(kind: kind, bytes: bytes)
            case 3: return .binary(bytes)
            case 4:
                guard bytes.count == 4 else { return .raw(kind: kind, bytes: bytes) }
                return .dword(bytes.reversed().reduce(0) { $0 << 8 | UInt32($1) })
            case 5:
                guard bytes.count >= 4 else { return .raw(kind: kind, bytes: bytes) }
                return .dword(bytes.prefix(4).reduce(0) { $0 << 8 | UInt32($1) })
            case 7:
                guard let flat = text() else { return .raw(kind: kind, bytes: bytes) }
                return .multiSz(flat.split(separator: "\0").map(String.init).joined(separator: "\n"))
            case 0xb:
                guard bytes.count == 8 else { return .raw(kind: kind, bytes: bytes) }
                return .qword(bytes.reversed().reduce(0) { $0 << 8 | UInt64($1) })
            case 0, 6, 8, 9, 0xa: return .raw(kind: kind, bytes: bytes)
            // regashii has no kind for other numbers, and Ludusavi drops such values.
            default: return nil
            }
        }
    }

    private static func utf16String(_ bytes: [UInt8]) -> String? {
        guard bytes.count % 2 == 0 else { return nil }
        let units = stride(from: 0, to: bytes.count, by: 2).map { UInt16(bytes[$0]) | UInt16(bytes[$0 + 1]) << 8 }
        var text = ""
        for scalar in Unicode.UTF16.decode(units) {
            guard let scalar else { return nil }
            text.unicodeScalars.append(scalar)
        }
        return trimNulls(text)
    }

    private static func trimNulls(_ text: String) -> String {
        var trimmed = Substring(text)
        while trimmed.last == "\0" { trimmed = trimmed.dropLast() }
        return String(trimmed)
    }
}

private extension Unicode.UTF16 {
    /// Each scalar, or nil for an unpaired surrogate.
    static func decode(_ units: [UInt16]) -> [Unicode.Scalar?] {
        var result: [Unicode.Scalar?] = []
        var iterator = units.makeIterator()
        var decoder = Unicode.UTF16()
        loop: while true {
            switch decoder.decode(&iterator) {
            case let .scalarValue(scalar): result.append(scalar)
            case .emptyInput: break loop
            case .error: result.append(nil)
            }
        }
        return result
    }
}
