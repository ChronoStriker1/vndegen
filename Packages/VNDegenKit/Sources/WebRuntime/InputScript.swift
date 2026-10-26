#if DEBUG
import CoreGraphics
import Foundation
import VNCore

/// A scripted play session for debug builds, so a game can be driven with no person at the controls.
///
/// Launch with `-<engine>-script "<steps>"`, such as `-rpgmaker-script`, where the value is either the steps themselves
/// or a file holding them.
/// A file holds one step per line, so `js` steps can use semicolons. Inline steps are separated by semicolons.
/// A `#` line is a comment:
///
///     wait 2.5            sleep
///     press confirm       press and release a VirtualButton (100 ms, longer than one engine frame)
///     hold right 1.0      hold a button for the given seconds
///     tap 0.5 0.5         pointer down and up at normalized view coordinates
///     scroll -30          one wheel event
///     js <expression>     evaluate in the page and log the result
///     fps 5               count animation frames and engine updates for the given seconds and log the rates
///     log <text>          write a marker to the app log
///     quit                end the session the way the game's Quit command would
nonisolated enum InputStep: Equatable, Sendable {
    case wait(Double)
    case press(VirtualButton)
    case hold(VirtualButton, Double)
    case tap(CGPoint)
    case scroll(Double)
    case js(String)
    case fps(Double)
    case log(String)
    case quit
}

nonisolated struct InputScriptError: Error, Equatable {
    let line: String
}

nonisolated enum InputScript {
    static func parse(_ text: String, inline: Bool = false) throws -> [InputStep] {
        try text.split(whereSeparator: { $0 == "\n" || (inline && $0 == ";") })
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty && !$0.hasPrefix("#") }
            .map(step)
    }

    /// Reads the launch argument. The value is a path when a file exists there, otherwise the steps inline.
    static func fromLaunchArguments(_ argument: String, defaults: UserDefaults = .standard) throws -> [InputStep]? {
        guard let raw = defaults.string(forKey: argument) else { return nil }
        if let file = try? String(contentsOfFile: raw, encoding: .utf8) { return try parse(file) }
        return try parse(raw, inline: true)
    }

    private static func step(_ line: String) throws -> InputStep {
        let words = line.split(separator: " ", omittingEmptySubsequences: true).map(String.init)
        let rest = words.dropFirst().joined(separator: " ")
        switch words.first {
        case "wait": if let s = Double(rest) { return .wait(s) }
        case "press": if let b = VirtualButton(rawValue: rest) { return .press(b) }
        case "hold": if words.count == 3, let b = VirtualButton(rawValue: words[1]), let s = Double(words[2]) { return .hold(b, s) }
        case "tap": if words.count == 3, let x = Double(words[1]), let y = Double(words[2]) { return .tap(CGPoint(x: x, y: y)) }
        case "scroll": if let dy = Double(rest) { return .scroll(dy) }
        case "js": if !rest.isEmpty { return .js(rest) }
        case "fps": if let s = Double(rest), s > 0 { return .fps(s) }
        case "log": return .log(rest)
        case "quit": return .quit
        default: break
        }
        throw InputScriptError(line: line)
    }
}
#endif
