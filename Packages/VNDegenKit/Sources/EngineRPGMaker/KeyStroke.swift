import VNCore

/// The keyboard key RPG Maker reads for a virtual button, from MZ's `Input.keyMapper`. MV uses the same table.
/// Cancel and menu both reach "escape" in the engine, but X and Escape stay distinct so keyboard plugins see what a
/// keyboard player would press. Games read controllers as a gamepad, so only DEBUG input scripts press these, and
/// remaps saved before then convert to them.
nonisolated enum KeyStroke {
    static func key(for button: VirtualButton) -> KeyboardKey? {
        switch button {
        case .up: KeyboardKey(key: "ArrowUp", code: "ArrowUp", keyCode: 38)
        case .down: KeyboardKey(key: "ArrowDown", code: "ArrowDown", keyCode: 40)
        case .left: KeyboardKey(key: "ArrowLeft", code: "ArrowLeft", keyCode: 37)
        case .right: KeyboardKey(key: "ArrowRight", code: "ArrowRight", keyCode: 39)
        case .confirm: KeyboardKey(key: "Enter", code: "Enter", keyCode: 13)
        case .cancel: KeyboardKey(key: "Escape", code: "Escape", keyCode: 27)
        case .menu: KeyboardKey(key: "x", code: "KeyX", keyCode: 88)
        case .dash: KeyboardKey(key: "Shift", code: "ShiftLeft", keyCode: 16)
        case .pageUp: KeyboardKey(key: "q", code: "KeyQ", keyCode: 81)
        case .pageDown: KeyboardKey(key: "w", code: "KeyW", keyCode: 87)
        case .hub: nil
        }
    }
}
