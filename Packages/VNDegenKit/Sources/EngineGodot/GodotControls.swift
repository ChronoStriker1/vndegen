import Foundation
import VNCore

nonisolated enum GodotInputEvent: Equatable, Sendable {
    case key(Int)
    case joyButton(Int)
    case joyAxis(Int, Double)
    case mouse(Int)
}

nonisolated struct GodotAction: Equatable, Sendable {
    var name: String
    var events: [GodotInputEvent]
}

nonisolated struct GodotScript: Equatable, Sendable {
    var name: String
    var offset: UInt64
    var size: UInt64
    var encrypted: Bool
}

nonisolated enum GodotControls {
    static let version = 1

    static let joypadReads = ["is_joy_button_pressed", "get_joy_axis", "InputEventJoypadButton", "InputEventJoypadMotion"]

    static func readsJoypad(_ scripts: [GodotScript], read: (UInt64, Int) -> Data?) -> Bool? {
        guard !scripts.isEmpty else { return nil }
        for script in scripts {
            guard !script.encrypted, !script.name.lowercased().hasSuffix(".gde"), script.size < 1 << 26,
                  let data = read(script.offset, Int(script.size)) else { return nil }
            guard let reads = readsJoypad(data) else { return nil }
            if reads { return true }
        }
        return false
    }

    static func readsJoypad(_ script: Data) -> Bool? {
        let words = joypadReads.map { Data($0.utf8) }
        guard script.starts(with: Data("GDSC".utf8)) else { return words.contains { script.range(of: $0) != nil } }
        guard script.count >= 12 else { return nil }
        let field = { (at: Int) in (0..<4).reduce(UInt32(0)) { $0 | UInt32(script[script.startIndex + at + $1]) << (8 * $1) } }
        if field(4) >= 100 {
            guard field(8) == 0 else { return nil }
            return words.contains { word in
                script.range(of: Data(word.flatMap { byte in [byte ^ 0xB6, 0xB6, 0xB6, 0xB6] })) != nil
            }
        }
        return words.contains { script.range(of: Data($0.map { $0 ^ 0xB6 })) != nil }
    }

    static func inputMap(_ data: Data, major: Int) -> [GodotAction] {
        guard data.count >= 8, data.prefix(4) == Data("ECFG".utf8) else { return [] }
        var reader = VariantReader(data: data, cursor: 4, major: major)
        var result: [GodotAction] = []
        guard let count = reader.uint32() else { return [] }
        for _ in 0..<min(Int(count), 100_000) {
            guard let keyLength = reader.uint32(), let key = reader.bytes(Int(keyLength)), let valueLength = reader.uint32() else { break }
            let end = reader.cursor + Int(valueLength)
            let name = String(decoding: key, as: UTF8.self)
            if name.hasPrefix("input/"), let value = try? reader.variant(), case let .dictionary(entries) = value {
                let events = entries.first { $0.0 == .string("events") }?.1
                if case let .array(list) = events { result.append(GodotAction(name: String(name.dropFirst(6)), events: list.compactMap(event))) }
            }
            reader.cursor = end
        }
        return result
    }

    private static func event(_ value: Variant) -> GodotInputEvent? {
        guard case let .object(type, properties) = value else { return nil }
        let int = { (name: String) -> Int? in if case let .int(value) = properties[name] { value } else { nil } }
        switch type {
        case "InputEventKey":
            let modifiers = ["shift_pressed", "ctrl_pressed", "alt_pressed", "meta_pressed", "shift", "control", "alt", "meta", "command"]
            guard !modifiers.contains(where: { properties[$0] == .bool(true) }) else { return nil }
            let code = [int("keycode"), int("physical_keycode"), int("scancode"), int("physical_scancode")].compactMap(\.self).first { $0 != 0 }
            return code.map(GodotInputEvent.key)
        case "InputEventJoypadButton":
            return int("button_index").map(GodotInputEvent.joyButton)
        case "InputEventJoypadMotion":
            guard let axis = int("axis") else { return nil }
            let value: Double = switch properties["axis_value"] {
            case let .float(value): value
            case let .int(value): Double(value)
            default: 0
            }
            return .joyAxis(axis, value)
        case "InputEventMouseButton":
            return int("button_index").map(GodotInputEvent.mouse)
        default:
            return nil
        }
    }

    static func builtIns(major: Int) -> [String: [GodotInputEvent]] {
        let special = major >= 4 ? 1 << 22 : 1 << 24
        let enter = special + 5, keypadEnter = special + 6, escape = special + 1, space = 32
        let left = special + 0x0F, up = special + 0x10, right = special + 0x11, down = special + 0x12
        if major >= 4 {
            return [
                "ui_accept": [.key(enter), .key(keypadEnter), .key(space), .joyButton(0)],
                "ui_select": [.key(space), .joyButton(3)],
                "ui_cancel": [.key(escape), .joyButton(1)],
                "ui_left": [.key(left), .joyButton(13), .joyAxis(0, -1)],
                "ui_right": [.key(right), .joyButton(14), .joyAxis(0, 1)],
                "ui_up": [.key(up), .joyButton(11), .joyAxis(1, -1)],
                "ui_down": [.key(down), .joyButton(12), .joyAxis(1, 1)],
            ]
        }
        return [
            "ui_accept": [.key(enter), .key(keypadEnter), .key(space), .joyButton(0)],
            "ui_select": [.key(space), .joyButton(3)],
            "ui_cancel": [.key(escape), .joyButton(1)],
            "ui_left": [.key(left), .joyButton(14)],
            "ui_right": [.key(right), .joyButton(15)],
            "ui_up": [.key(up), .joyButton(12)],
            "ui_down": [.key(down), .joyButton(13)],
        ]
    }

    static func inputs(_ event: GodotInputEvent, major: Int) -> [ControlInput] {
        switch event {
        case let .joyButton(index):
            let buttons: [ControlInput?] = major >= 4
                ? [.a, .b, .x, .y, .view, nil, .menu, .l3, .r3, .l, .r, .up, .down, .left, .right]
                : [.a, .b, .x, .y, .l, .r, .l2, .r2, .l3, .r3, .view, .menu, .up, .down, .left, .right]
            return buttons.indices.contains(index) ? buttons[index].map { [$0] } ?? [] : []
        case let .joyAxis(axis, value):
            let triggers = major >= 4 ? (4, 5) : (6, 7)
            let sticks: [Int: (ControlInput, ControlInput)] = [
                0: (.leftStickLeft, .leftStickRight), 1: (.leftStickUp, .leftStickDown),
                2: (.rightStickLeft, .rightStickRight), 3: (.rightStickUp, .rightStickDown),
            ]
            if axis == triggers.0 { return [.l2] }
            if axis == triggers.1 { return [.r2] }
            guard let (negative, positive) = sticks[axis] else { return [] }
            return value < 0 ? [negative] : value > 0 ? [positive] : [negative, positive]
        case .key, .mouse:
            return []
        }
    }

    static let fillOrder: [ControlInput] = [.x, .y, .r, .l, .r2, .l2, .view, .l3, .r3, .menu]
    static let directions: [ControlInput] = [.up, .down, .left, .right, .leftStickUp, .leftStickDown, .leftStickLeft,
                                             .leftStickRight, .rightStickUp, .rightStickDown, .rightStickLeft, .rightStickRight]
    static let editingActions: Set<String> = ["ui_menu", "ui_home", "ui_end", "ui_cut", "ui_copy", "ui_paste", "ui_undo", "ui_redo", "ui_unicode_start"]
    static let editingPrefixes = ["ui_focus_", "ui_text_", "ui_graph_", "ui_filedialog_", "ui_swap_", "ui_colorpicker_", "ui_accessibility_"]

    static func isSkipped(_ name: String) -> Bool {
        editingActions.contains(name) || editingPrefixes.contains(where: name.hasPrefix)
            || name.range(of: "debug|develop|(^|_)dev($|_)|cheat|test|console|translator|bug_report|screenshot",
                          options: [.regularExpression, .caseInsensitive]) != nil
    }

    static func windowsKeyCode(_ code: Int, major: Int) -> Int? {
        let special = major >= 4 ? 1 << 22 : 1 << 24
        if code < special { return (65...90).contains(code) || (48...57).contains(code) || code == 32 ? code : nil }
        let named: [Int: Int] = [0x01: 27, 0x02: 9, 0x04: 8, 0x05: 13, 0x0D: 36, 0x0E: 35, 0x0F: 37, 0x10: 38, 0x11: 39,
                                 0x12: 40, 0x13: 33, 0x14: 34, 0x15: 16, 0x16: 17, 0x18: 18]
        let offset = code - special
        if (0x1C...0x27).contains(offset) { return 112 + offset - 0x1C }
        return named[offset]
    }

    static func suggestion(_ map: [GodotAction], major: Int) -> ControlSuggestion? {
        var actions = builtIns(major: major)
        for action in map { actions[action.name] = action.events }
        var readers: [ControlInput: [[GodotInputEvent]]] = [:]
        for events in actions.values {
            for input in Set(events.flatMap { inputs($0, major: major) }) { readers[input, default: []].append(events) }
        }
        let free = (fillOrder + directions).filter { readers[$0] == nil }
        let readsJoypad = { (events: [GodotInputEvent]) in events.contains { !inputs($0, major: major).isEmpty } }

        let pointer = [ControlInput.rightStickUp, .rightStickDown, .rightStickLeft, .rightStickRight]
        let hasPointer = pointer.allSatisfy(free.contains)
        var bindings: [ControlSuggestion.Binding] = []
        var usesMouse = false
        for (name, events) in map.map({ ($0.name, $0.events) }) where !isSkipped(name) && !readsJoypad(events) {
            guard let (event, action) = events.lazy.compactMap({ event in
                Self.action(event, major: major).map { (event, $0) }
            }).first else { continue }
            if case .mouse = event {
                guard hasPointer else { continue }
                usesMouse = true
            }
            let preferred = preferences(action)
            let carrying = preferred.filter { input in readers[input]?.allSatisfy { $0.contains(event) } ?? false }
            bindings.append(.init(action, label: label(name), on: preferred.filter(free.contains), carrying: carrying))
        }
        guard var suggestion = ControlSuggestion(placing: bindings, on: free) else { return nil }
        if usesMouse, suggestion.buttons.values.contains(where: { if case .click = $0 { true } else { false } }) {
            for (input, direction) in zip(pointer, PointerDirection.allCases) { suggestion.buttons[input] = .mouseMove(direction) }
        }
        return suggestion
    }

    private static func action(_ event: GodotInputEvent, major: Int) -> ControlAction? {
        switch event {
        case let .key(code): windowsKeyCode(code, major: major).flatMap(KeyboardKey.init(windowsKeyCode:)).map(ControlAction.key)
        case .mouse(1): .click(.primary)
        case .mouse(2): .click(.secondary)
        case .mouse(4): .wheel(.up)
        case .mouse(5): .wheel(.down)
        default: nil
        }
    }

    private static func preferences(_ action: ControlAction) -> [ControlInput] {
        switch action {
        case let .key(key):
            switch key.keyCode {
            case 38: [.up, .leftStickUp]
            case 40: [.down, .leftStickDown]
            case 37: [.left, .leftStickLeft]
            case 39: [.right, .leftStickRight]
            case 87: [.leftStickUp, .up]
            case 83: [.leftStickDown, .down]
            case 65: [.leftStickLeft, .left]
            case 68: [.leftStickRight, .right]
            case 13, 32: [.a]
            case 27: [.menu]
            case 9: [.view]
            default: []
            }
        case .click(.primary): [.r2]
        case .click(.secondary): [.l2]
        case .wheel(.up): [.r]
        case .wheel(.down): [.l]
        default: []
        }
    }

    static func label(_ name: String) -> String {
        let base = name.hasPrefix("ui_") ? String(name.dropFirst(3)) : name
        let words = base.replacingOccurrences(of: "_", with: " ").trimmingCharacters(in: .whitespaces)
        let plain = words == words.uppercased() ? words.lowercased() : words
        return plain.prefix(1).uppercased() + plain.dropFirst()
    }
}

nonisolated indirect enum Variant: Equatable {
    case null
    case bool(Bool)
    case int(Int)
    case float(Double)
    case string(String)
    case object(String, [String: Variant])
    case dictionary([(Variant, Variant)])
    case array([Variant])
    case other

    static func == (lhs: Variant, rhs: Variant) -> Bool {
        switch (lhs, rhs) {
        case (.null, .null), (.other, .other): true
        case let (.bool(a), .bool(b)): a == b
        case let (.int(a), .int(b)): a == b
        case let (.float(a), .float(b)): a == b
        case let (.string(a), .string(b)): a == b
        case let (.object(a, x), .object(b, y)): a == b && x == y
        case let (.array(a), .array(b)): a == b
        case let (.dictionary(a), .dictionary(b)): a.count == b.count && zip(a, b).allSatisfy { $0.0 == $1.0 && $0.1 == $1.1 }
        default: false
        }
    }
}

nonisolated struct VariantReader {
    struct Malformed: Error {}

    let data: Data
    var cursor: Int
    let major: Int

    mutating func bytes(_ count: Int) -> Data? {
        guard count >= 0, cursor + count <= data.count else { return nil }
        defer { cursor += count }
        return data.subdata(in: (data.startIndex + cursor)..<(data.startIndex + cursor + count))
    }

    mutating func uint32() -> UInt32? {
        bytes(4).map { $0.enumerated().reduce(0) { $0 | UInt32($1.element) << (8 * $1.offset) } }
    }

    mutating func uint64() -> UInt64? {
        bytes(8).map { $0.enumerated().reduce(0) { $0 | UInt64($1.element) << (8 * $1.offset) } }
    }

    mutating func string() throws -> String {
        guard let length = uint32(), let text = bytes(Int(length)) else { throw Malformed() }
        cursor += (4 - Int(length) % 4) % 4
        return String(decoding: text, as: UTF8.self)
    }

    private mutating func skip(_ count: Int) throws {
        guard bytes(count) != nil else { throw Malformed() }
    }

    mutating func variant(depth: Int = 0) throws -> Variant {
        guard depth < 32, let header = uint32() else { throw Malformed() }
        let type = Int(header & 0xFF), wide = header & (1 << 16) != 0
        let real = wide ? 8 : 4
        if major >= 4 {
            switch type {
            case 0: return .null
            case 1: return .bool((uint32() ?? 0) != 0)
            case 2: return try integer(wide)
            case 3: return try float(wide)
            case 4, 21: return .string(try string())
            case 5: try skip(2 * real)
            case 6: try skip(8)
            case 7: try skip(4 * real)
            case 8, 13: try skip(16)
            case 9: try skip(3 * real)
            case 10: try skip(12)
            case 11: try skip(6 * real)
            case 12, 14, 15: try skip(4 * real)
            case 16: try skip(6 * real)
            case 17: try skip(9 * real)
            case 18: try skip(12 * real)
            case 19: try skip(16 * real)
            case 20: try skip(16)
            case 23: try skip(8)
            case 24: return try object(objectAsID: wide, depth: depth)
            case 27: return try dictionary(header, depth: depth)
            case 28: return try array(header, depth: depth)
            default: throw Malformed()
            }
        } else {
            switch type {
            case 0: return .null
            case 1: return .bool((uint32() ?? 0) != 0)
            case 2: return try integer(wide)
            case 3: return try float(wide)
            case 4: return .string(try string())
            case 5: try skip(8)
            case 6, 9, 10, 14: try skip(16)
            case 7: try skip(12)
            case 8, 11: try skip(24)
            case 12: try skip(36)
            case 13: try skip(48)
            case 16: try skip(8)
            case 17: return try object(objectAsID: wide, depth: depth)
            case 18: return try dictionary(header, depth: depth)
            case 19: return try array(header, depth: depth)
            default: throw Malformed()
            }
        }
        return .other
    }

    private mutating func integer(_ wide: Bool) throws -> Variant {
        if wide { guard let value = uint64() else { throw Malformed() }; return .int(Int(Int64(bitPattern: value))) }
        guard let value = uint32() else { throw Malformed() }
        return .int(Int(Int32(bitPattern: value)))
    }

    private mutating func float(_ wide: Bool) throws -> Variant {
        if wide { guard let value = uint64() else { throw Malformed() }; return .float(Double(bitPattern: value)) }
        guard let value = uint32() else { throw Malformed() }
        return .float(Double(Float(bitPattern: value)))
    }

    private mutating func object(objectAsID: Bool, depth: Int) throws -> Variant {
        if objectAsID {
            try skip(8)
            return .null
        }
        let type = try string()
        guard !type.isEmpty else { return .null }
        guard let count = uint32() else { throw Malformed() }
        var properties: [String: Variant] = [:]
        for _ in 0..<count {
            let name = try string()
            properties[name] = try variant(depth: depth + 1)
        }
        return .object(type, properties)
    }

    private mutating func containerType(_ kind: UInt32) throws {
        switch kind {
        case 1: try skip(4)
        case 2, 3: _ = try string()
        default: break
        }
    }

    private mutating func dictionary(_ header: UInt32, depth: Int) throws -> Variant {
        if major >= 4 {
            try containerType((header >> 16) & 3)
            try containerType((header >> 18) & 3)
        }
        guard let count = uint32() else { throw Malformed() }
        var entries: [(Variant, Variant)] = []
        for _ in 0..<(count & 0x7FFF_FFFF) {
            entries.append((try variant(depth: depth + 1), try variant(depth: depth + 1)))
        }
        return .dictionary(entries)
    }

    private mutating func array(_ header: UInt32, depth: Int) throws -> Variant {
        if major >= 4 { try containerType((header >> 16) & 3) }
        guard let count = uint32() else { throw Malformed() }
        var items: [Variant] = []
        for _ in 0..<(count & 0x7FFF_FFFF) { items.append(try variant(depth: depth + 1)) }
        return .array(items)
    }
}
