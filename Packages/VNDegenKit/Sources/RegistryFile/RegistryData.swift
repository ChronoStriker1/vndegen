import Foundation

/// A registry value as Ludusavi records it, in its `registry.yaml` and in the `registry.reg` it writes through regashii.
public enum RegistryValue: Equatable, Sendable {
    case sz(String)
    case expandSz(String)
    /// The strings joined by newlines, as Ludusavi keeps them.
    case multiSz(String)
    case dword(UInt32)
    case qword(UInt64)
    case binary([UInt8])
    /// Any other kind, as its bytes. `kind` is the Windows type number, such as 4 for a DWORD.
    case raw(kind: UInt8, bytes: [UInt8])
}

/// Registry keys and their values: Ludusavi's `Hives`, with each key named in full, such as
/// `HKEY_CURRENT_USER\Software\Company\Game`. Names compare without case, as Windows and regashii compare them, and keep
/// the spelling they were first seen with.
public struct RegistryData: Equatable, Sendable {
    public private(set) var keys: [String: [String: RegistryValue]] = [:]

    public init() {}

    public static let regedit5Header = "Windows Registry Editor Version 5.00"
    static let regedit4Header = "REGEDIT4"

    public var isEmpty: Bool { keys.isEmpty }

    /// The values of `key`, matched without case. Only that key's own, never its subkeys'.
    public func values(of key: String) -> [String: RegistryValue]? {
        spelling(of: key).flatMap { keys[$0] }
    }

    /// Replaces the values of `key`, keeping the spelling it already has. Every other key stays as it is.
    public mutating func setValues(_ values: [String: RegistryValue], of key: String) {
        let name = spelling(of: key) ?? Self.normalized(key)
        var merged: [String: RegistryValue] = [:]
        for (valueName, value) in values.sortedByUTF8 { Self.insert(value, named: valueName, into: &merged) }
        keys[name] = merged
    }

    public mutating func removeKey(_ key: String) {
        if let name = spelling(of: key) { keys[name] = nil }
    }

    /// A copy holding only `key`, or an empty one when it isn't there.
    public func only(_ key: String) -> RegistryData {
        var copy = RegistryData()
        if let name = spelling(of: key), let values = keys[name] { copy.keys[name] = values }
        return copy
    }

    private func spelling(of key: String) -> String? {
        let wanted = Self.normalized(key).lowercased()
        return keys.keys.first { $0.lowercased() == wanted }
    }

    /// regashii's `KeyName::new`: no trailing backslash, and no doubled ones.
    static func normalized(_ key: String) -> String {
        var name = String(key.reversed().drop { $0 == "\\" }.reversed())
        while name.contains("\\\\") { name = name.replacingOccurrences(of: "\\\\", with: "\\") }
        return name
    }

    /// Adds `values` under `key`, merging with a key spelled otherwise, as regashii's `Registry::insert` merges keys
    /// added twice.
    mutating func add(_ values: [String: RegistryValue], to key: String) {
        let name = spelling(of: key) ?? Self.normalized(key)
        var merged = keys[name] ?? [:]
        for (valueName, value) in values.sortedByUTF8 { Self.insert(value, named: valueName, into: &merged) }
        keys[name] = merged
    }

    /// A later value replaces one whose name differs only in case, under the name first seen.
    private static func insert(_ value: RegistryValue, named name: String, into values: inout [String: RegistryValue]) {
        let lower = name.lowercased()
        let existing = values.keys.first { $0.lowercased() == lower }
        values[existing ?? name] = value
    }
}

extension Dictionary where Key == String {
    /// Sorted as Rust's `BTreeMap<String, _>` sorts, by UTF-8 bytes, which is the order regashii writes.
    var sortedByUTF8: [(key: String, value: Value)] {
        sorted { Array($0.key.utf8).lexicographicallyPrecedes(Array($1.key.utf8)) }
    }
}
