public struct ControlSuggestion: Equatable, Codable, Sendable {
    public var buttons: [ControlInput: ControlAction]
    public var labels: [ControlInput: String]

    public init(buttons: [ControlInput: ControlAction], labels: [ControlInput: String] = [:]) {
        self.buttons = buttons
        self.labels = labels
    }

    public struct Binding: Equatable, Sendable {
        public var action: ControlAction
        public var label: String?
        public var on: [ControlInput]
        public var carrying: [ControlInput]
        public var fallback: Bool

        public init(_ action: ControlAction, label: String? = nil, on: [ControlInput] = [], carrying: [ControlInput] = [],
                    fallback: Bool = true) {
            self.action = action
            self.label = label
            self.on = on
            self.carrying = carrying
            self.fallback = fallback
        }
    }

    static let directions: Set<ControlInput> = [
        .up, .down, .left, .right, .leftStickUp, .leftStickDown, .leftStickLeft, .leftStickRight,
        .rightStickUp, .rightStickDown, .rightStickLeft, .rightStickRight,
    ]

    public init?(placing bindings: [Binding], on free: [ControlInput]) {
        var open = free
        var buttons: [ControlInput: ControlAction] = [:]
        var labels: [ControlInput: String] = [:]
        var placed: Set<ControlAction> = []
        func place(_ binding: Binding, _ input: ControlInput) {
            open.removeAll { $0 == input }
            buttons[input] = binding.action
            if let label = binding.label { labels[input] = label }
            placed.insert(binding.action)
        }
        var rest: [Binding] = []
        for binding in bindings where !placed.contains(binding.action) {
            let preferred = binding.on.filter(open.contains) + binding.carrying.filter { buttons[$0] == nil && !open.contains($0) }
            preferred.forEach { place(binding, $0) }
            if preferred.isEmpty, binding.fallback { rest.append(binding) }
        }
        for binding in rest where !placed.contains(binding.action) {
            guard let input = open.first(where: { !Self.directions.contains($0) }) else { break }
            place(binding, input)
        }
        guard !buttons.isEmpty else { return nil }
        self.init(buttons: buttons, labels: labels)
    }
}

extension KeyboardKey {
    public init?(windowsKeyCode code: Int) {
        let named: [Int: (String, String)] = [
            8: ("Backspace", "Backspace"), 9: ("Tab", "Tab"), 13: ("Enter", "Enter"), 16: ("Shift", "ShiftLeft"),
            17: ("Control", "ControlLeft"), 18: ("Alt", "AltLeft"), 27: ("Escape", "Escape"), 32: (" ", "Space"),
            33: ("PageUp", "PageUp"), 34: ("PageDown", "PageDown"), 35: ("End", "End"), 36: ("Home", "Home"),
            37: ("ArrowLeft", "ArrowLeft"), 38: ("ArrowUp", "ArrowUp"), 39: ("ArrowRight", "ArrowRight"),
            40: ("ArrowDown", "ArrowDown"),
        ]
        switch code {
        case 65...90:
            let letter = String(UnicodeScalar(UInt8(code)))
            self.init(key: letter.lowercased(), code: "Key\(letter)", keyCode: code)
        case 48...57:
            self.init(key: "\(code - 48)", code: "Digit\(code - 48)", keyCode: code)
        case 112...123:
            self.init(key: "F\(code - 111)", code: "F\(code - 111)", keyCode: code)
        default:
            guard let (key, name) = named[code] else { return nil }
            self.init(key: key, code: name, keyCode: code)
        }
    }
}
