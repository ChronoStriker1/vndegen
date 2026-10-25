import VNCore

/// The keyboard key RPG Maker reads for a virtual button, from MZ's `Input.keyMapper`. MV uses the same table.
/// Cancel and menu both reach "escape" in the engine, but X and Escape stay distinct so keyboard plugins see what a
/// keyboard player would press.
nonisolated struct KeyStroke: Equatable, Sendable {
    let key: String
    let code: String
    let keyCode: Int

    static func stroke(for button: VirtualButton) -> KeyStroke? {
        switch button {
        case .up: KeyStroke(key: "ArrowUp", code: "ArrowUp", keyCode: 38)
        case .down: KeyStroke(key: "ArrowDown", code: "ArrowDown", keyCode: 40)
        case .left: KeyStroke(key: "ArrowLeft", code: "ArrowLeft", keyCode: 37)
        case .right: KeyStroke(key: "ArrowRight", code: "ArrowRight", keyCode: 39)
        case .confirm: KeyStroke(key: "Enter", code: "Enter", keyCode: 13)
        case .cancel: KeyStroke(key: "Escape", code: "Escape", keyCode: 27)
        case .menu: KeyStroke(key: "x", code: "KeyX", keyCode: 88)
        case .dash: KeyStroke(key: "Shift", code: "ShiftLeft", keyCode: 16)
        case .pageUp: KeyStroke(key: "q", code: "KeyQ", keyCode: 81)
        case .pageDown: KeyStroke(key: "w", code: "KeyW", keyCode: 87)
        case .hub: nil
        }
    }
}
