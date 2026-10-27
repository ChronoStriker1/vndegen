import CGodot
import Foundation

/// The engine's C functions from `vngd.h`, in one of the Godot frameworks the app embeds: Godot3 for Godot 3 games and
/// Godot4 for Godot 4 games. Both export the same names, so each is opened on its own and its functions are looked up
/// in it. Nil when this device or build of VNDegen has no such engine.
struct GodotEngine {
    let start: @convention(c) (UnsafePointer<VNGDLaunch>?) -> Int32
    let iterate: @convention(c) () -> Int32
    let resize: @convention(c) (Float, Float, Float) -> Void
    let key: @convention(c) (Int32, Bool) -> Void
    let text: @convention(c) (UnsafePointer<CChar>?) -> Void
    let mouseMove: @convention(c) (Float, Float) -> Void
    let mouseButton: @convention(c) (Int32, Bool) -> Void
    let wheel: @convention(c) (Float) -> Void
    let touch: @convention(c) (Int32, Int32, Float, Float) -> Void
    let joyButton: @convention(c) (Int32, Bool) -> Void
    let joyAxis: @convention(c) (Int32, Float) -> Void
    let setPaused: @convention(c) (Bool) -> Void
    let requestQuit: @convention(c) () -> Void
    let stop: @convention(c) () -> Void
    let problem: @convention(c) () -> UnsafePointer<CChar>?

    /// The framework's binary inside the app, such as Godot4.framework/Godot4.
    static func binary(major: Int) -> URL? {
        Bundle.main.privateFrameworksURL?.appendingPathComponent("Godot\(major).framework/Godot\(major)")
    }

    @MainActor private static var loaded: [Int: GodotEngine?] = [:]

    @MainActor static func engine(major: Int) -> GodotEngine? {
        if let engine = loaded[major] { return engine }
        let engine = load(major: major)
        loaded[major] = engine
        return engine
    }

    private static func load(major: Int) -> GodotEngine? {
        #if arch(arm64)
        guard let path = binary(major: major)?.path, let handle = dlopen(path, RTLD_NOW | RTLD_LOCAL) else { return nil }
        func function<T>(_ name: String) -> T? {
            dlsym(handle, name).map { unsafeBitCast($0, to: T.self) }
        }
        guard let start: @convention(c) (UnsafePointer<VNGDLaunch>?) -> Int32 = function("vngd_start"),
              let iterate: @convention(c) () -> Int32 = function("vngd_iterate"),
              let resize: @convention(c) (Float, Float, Float) -> Void = function("vngd_resize"),
              let key: @convention(c) (Int32, Bool) -> Void = function("vngd_key"),
              let text: @convention(c) (UnsafePointer<CChar>?) -> Void = function("vngd_text"),
              let mouseMove: @convention(c) (Float, Float) -> Void = function("vngd_mouse_move"),
              let mouseButton: @convention(c) (Int32, Bool) -> Void = function("vngd_mouse_button"),
              let wheel: @convention(c) (Float) -> Void = function("vngd_wheel"),
              let touch: @convention(c) (Int32, Int32, Float, Float) -> Void = function("vngd_touch"),
              let joyButton: @convention(c) (Int32, Bool) -> Void = function("vngd_joy_button"),
              let joyAxis: @convention(c) (Int32, Float) -> Void = function("vngd_joy_axis"),
              let setPaused: @convention(c) (Bool) -> Void = function("vngd_set_paused"),
              let requestQuit: @convention(c) () -> Void = function("vngd_request_quit"),
              let stop: @convention(c) () -> Void = function("vngd_stop"),
              let problem: @convention(c) () -> UnsafePointer<CChar>? = function("vngd_problem")
        else { return nil }
        return GodotEngine(start: start, iterate: iterate, resize: resize, key: key, text: text, mouseMove: mouseMove,
                           mouseButton: mouseButton, wheel: wheel, touch: touch, joyButton: joyButton, joyAxis: joyAxis,
                           setPaused: setPaused, requestQuit: requestQuit, stop: stop, problem: problem)
        #else
        return nil
        #endif
    }

    var lastProblem: String? { problem().map { String(cString: $0) } }
}
