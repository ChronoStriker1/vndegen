import CRGSS
import Foundation

/// The engine's C functions from `vnrgss.h`, in the RGSS framework the app embeds. The app doesn't link the framework,
/// because it is arm64 only and the Mac app is universal, so it is opened here at run time. On an Intel Mac, or in a
/// build without it, there is no engine.
struct RGSSEngine {
    let start: @convention(c) (UnsafePointer<VNRGSSLaunch>?) -> Int32
    let frame: @convention(c) (Double) -> Int32
    let key: @convention(c) (UInt32, Bool) -> Void
    let mouseMove: @convention(c) (Float, Float) -> Void
    let mouseButton: @convention(c) (Int32, Bool) -> Void
    let wheel: @convention(c) (Float) -> Void
    let setPaused: @convention(c) (Bool) -> Void
    let stop: @convention(c) () -> Void
    let problem: @convention(c) () -> UnsafePointer<CChar>?

    /// The framework's binary, inside the app.
    static var binary: URL? {
        Bundle.main.privateFrameworksURL?.appendingPathComponent("RGSS.framework/RGSS")
    }

    /// Nil when this device or build of VNDegen has no engine.
    static let shared: RGSSEngine? = {
        #if arch(arm64)
        guard let path = binary?.path, let handle = dlopen(path, RTLD_NOW | RTLD_LOCAL) else { return nil }
        func function<T>(_ name: String) -> T? {
            dlsym(handle, name).map { unsafeBitCast($0, to: T.self) }
        }
        guard let start: @convention(c) (UnsafePointer<VNRGSSLaunch>?) -> Int32 = function("vnrgss_start"),
              let frame: @convention(c) (Double) -> Int32 = function("vnrgss_frame"),
              let key: @convention(c) (UInt32, Bool) -> Void = function("vnrgss_key"),
              let mouseMove: @convention(c) (Float, Float) -> Void = function("vnrgss_mouse_move"),
              let mouseButton: @convention(c) (Int32, Bool) -> Void = function("vnrgss_mouse_button"),
              let wheel: @convention(c) (Float) -> Void = function("vnrgss_wheel"),
              let setPaused: @convention(c) (Bool) -> Void = function("vnrgss_set_paused"),
              let stop: @convention(c) () -> Void = function("vnrgss_stop"),
              let problem: @convention(c) () -> UnsafePointer<CChar>? = function("vnrgss_problem")
        else { return nil }
        return RGSSEngine(start: start, frame: frame, key: key, mouseMove: mouseMove, mouseButton: mouseButton,
                          wheel: wheel, setPaused: setPaused, stop: stop, problem: problem)
        #else
        return nil
        #endif
    }()

    func send(_ input: RGSSInput) {
        switch input {
        case let .key(code, down): key(code, down)
        case let .mouseMove(to): mouseMove(Float(to.x), Float(to.y))
        case let .mouseButton(button, down): mouseButton(button, down)
        case let .wheel(notches): wheel(Float(notches))
        }
    }

    var lastProblem: String? { problem().map { String(cString: $0) } }
}
