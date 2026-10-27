import CKiriKiri
import Darwin

/// The engine's C functions from `vnkk.h`, found at run time in the KiriKiri framework the app embeds. The package
/// never links the engine, so it builds and tests without it.
struct KiriKiriEngine {
    let start: @convention(c) (UnsafePointer<VNKKLaunch>?) -> Int32
    let iterate: @convention(c) () -> Int32
    let key: @convention(c) (Int32, Bool) -> Void
    let mouseButton: @convention(c) (Int32, Bool, Float, Float) -> Void
    let mouseMove: @convention(c) (Float, Float) -> Void
    let click: @convention(c) (Int32, Bool) -> Void
    let wheel: @convention(c) (Float) -> Void
    let text: @convention(c) (UnsafePointer<CChar>?) -> Void
    let setPaused: @convention(c) (Bool) -> Void
    let requestQuit: @convention(c) () -> Void
    let saveSystemData: @convention(c) () -> Void
    let stop: @convention(c) () -> Void
    let view: @convention(c) () -> UnsafeMutableRawPointer?

    /// Nil when this build of VNDegen has no engine.
    static func load() -> KiriKiriEngine? {
        // RTLD_DEFAULT, a C macro Swift cannot import.
        let everywhere = UnsafeMutableRawPointer(bitPattern: -2)
        func function<T>(_ name: String) -> T? {
            dlsym(everywhere, name).map { unsafeBitCast($0, to: T.self) }
        }
        guard let start: @convention(c) (UnsafePointer<VNKKLaunch>?) -> Int32 = function("vnkk_start"),
              let iterate: @convention(c) () -> Int32 = function("vnkk_iterate"),
              let key: @convention(c) (Int32, Bool) -> Void = function("vnkk_key"),
              let mouseButton: @convention(c) (Int32, Bool, Float, Float) -> Void = function("vnkk_mouse_button"),
              let mouseMove: @convention(c) (Float, Float) -> Void = function("vnkk_mouse_move"),
              let click: @convention(c) (Int32, Bool) -> Void = function("vnkk_click"),
              let wheel: @convention(c) (Float) -> Void = function("vnkk_wheel"),
              let text: @convention(c) (UnsafePointer<CChar>?) -> Void = function("vnkk_text"),
              let setPaused: @convention(c) (Bool) -> Void = function("vnkk_set_paused"),
              let requestQuit: @convention(c) () -> Void = function("vnkk_request_quit"),
              let saveSystemData: @convention(c) () -> Void = function("vnkk_save_system_data"),
              let stop: @convention(c) () -> Void = function("vnkk_stop"),
              let view: @convention(c) () -> UnsafeMutableRawPointer? = function("vnkk_view")
        else { return nil }
        return KiriKiriEngine(start: start, iterate: iterate, key: key, mouseButton: mouseButton, mouseMove: mouseMove,
                              click: click, wheel: wheel, text: text, setPaused: setPaused, requestQuit: requestQuit,
                              saveSystemData: saveSystemData, stop: stop, view: view)
    }

    func send(_ input: KiriKiriInput) {
        switch input {
        case let .key(scancode, down): key(scancode, down)
        case let .mouseButton(button, down, at): mouseButton(button, down, Float(at.x), Float(at.y))
        case let .mouseMove(to): mouseMove(Float(to.x), Float(to.y))
        case let .click(button, down): click(button, down)
        case let .wheel(notches): wheel(Float(notches))
        case let .text(string): string.withCString(text)
        }
    }
}
