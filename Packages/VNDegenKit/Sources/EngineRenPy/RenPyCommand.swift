import Foundation
import VNCore

/// The helper process that runs a Ren'Py game: the runtime's Python running VNDegen's `launcher.py`, and the environment
/// the launcher and `zz_vndegen.rpy` read.
nonisolated public struct RenPyCommand: Sendable {
    public var executable: URL
    public var arguments: [String]
    public var environment: [String: String]

    /// Where the game window opens.
    public enum Placement: Sendable, Equatable {
        case fullScreen
        /// The window's content area in screen points, with the origin at the top left of the main display, as SDL
        /// counts.
        case frame(CGRect)
    }

    /// The app's bundled runtimes, one folder per runtime named by its raw value. `scripts/renpy-runtime.sh` fetches
    /// them and an Xcode build phase copies them in.
    public static var bundledRuntimes: URL? { Bundle.main.resourceURL?.appendingPathComponent("RenPy", isDirectory: true) }

    /// `launcher.py` and `scripts/zz_vndegen.rpy`.
    @MainActor public static var support: URL? { Bundle.module.url(forResource: "Support", withExtension: nil) }

    public static func python(_ runtime: RenPyRuntime, in runtimes: URL) -> URL {
        runtimes.appendingPathComponent("\(runtime.rawValue)/lib/\(runtime.macFolder)/python")
    }

    public init(base: URL, runtime: RenPyRuntime, runtimes: URL, support: URL, dataDirectory: URL, placement: Placement?,
                buttons: [ControlInput: ControlAction]? = nil) {
        executable = Self.python(runtime, in: runtimes)
        arguments = [support.appendingPathComponent("launcher.py").path, base.path]
        var environment = ProcessInfo.processInfo.environment
        environment["PYTHONHOME"] = nil
        environment["PYTHONPATH"] = nil
        environment["PYTHONDONTWRITEBYTECODE"] = "1"
        environment["VNDEGEN_RENPY_RUNTIME"] = runtimes.appendingPathComponent(runtime.rawValue).path
        environment["VNDEGEN_DATA"] = dataDirectory.path
        switch placement {
        case .fullScreen?:
            environment["VNDEGEN_FULLSCREEN"] = "1"
        case let .frame(rect)?:
            environment["VNDEGEN_WINDOW"] = [rect.minX, rect.minY, rect.width, rect.height].map { String(Int($0.rounded())) }
                .joined(separator: ",")
        case nil:
            break
        }
        environment["VNDEGEN_BUTTONS"] = buttons.flatMap(Self.buttonsVariable)
        self.environment = environment
    }

    /// The remapped buttons as `zz_vndegen.rpy` reads them, such as `a=key:K_z,b=click:3,x=button:confirm,y=nothing`.
    /// Nil when nothing is remapped.
    static func buttonsVariable(_ buttons: [ControlInput: ControlAction]) -> String? {
        let items = ControlInput.allCases.compactMap { input -> String? in
            let action: String? = switch buttons[input] {
            case let .button(button)?: "button:\(button.rawValue)"
            case let .key(key)?: pygameKey(key).map { "key:\($0)" }
            case .click(.primary)?: "click:1"
            case .click(.secondary)?: "click:3"
            case .nothing?: "nothing"
            case nil: nil
            }
            return action.map { "\(input.rawValue)=\($0)" }
        }
        return items.isEmpty ? nil : items.joined(separator: ",")
    }

    /// The pygame constant for a key in `KeyboardKey.choices`, from its web `code`.
    static func pygameKey(_ key: KeyboardKey) -> String? {
        let code = key.code
        if code.hasPrefix("Key") { return "K_" + code.dropFirst(3).lowercased() }
        if code.hasPrefix("Digit") { return "K_" + code.dropFirst(5) }
        if code.first == "F", Int(code.dropFirst()) != nil { return "K_" + code }
        let named = [
            "ArrowUp": "K_UP", "ArrowDown": "K_DOWN", "ArrowLeft": "K_LEFT", "ArrowRight": "K_RIGHT",
            "Enter": "K_RETURN", "Escape": "K_ESCAPE", "Space": "K_SPACE", "ShiftLeft": "K_LSHIFT",
            "ControlLeft": "K_LCTRL", "AltLeft": "K_LALT", "Tab": "K_TAB", "Backspace": "K_BACKSPACE",
            "PageUp": "K_PAGEUP", "PageDown": "K_PAGEDOWN", "Home": "K_HOME", "End": "K_END",
        ]
        return named[code]
    }
}
