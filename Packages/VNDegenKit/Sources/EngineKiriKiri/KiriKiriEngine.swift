import CKiriKiri
import Darwin
import FontPack

/// The engine's C functions from `vnkk.h`, found at run time in the KiriKiri framework the app embeds. The package
/// never links the engine, so it builds and tests without it.
struct KiriKiriEngine {
    let setFonts: @convention(c) (UnsafePointer<CChar>?, UnsafePointer<UnsafePointer<CChar>?>?, Int32) -> Void
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
    let setMenuBar: @convention(c) (Int32) -> Void
    let menuBarShown: @convention(c) () -> Int32
    let menu: @convention(c) () -> UnsafePointer<CChar>?
    let menuClick: @convention(c) (UnsafePointer<CChar>?) -> Bool

    /// Nil when this build of VNDegen has no engine.
    static func load() -> KiriKiriEngine? {
        // RTLD_DEFAULT, a C macro Swift cannot import.
        let everywhere = UnsafeMutableRawPointer(bitPattern: -2)
        func function<T>(_ name: String) -> T? {
            dlsym(everywhere, name).map { unsafeBitCast($0, to: T.self) }
        }
        guard let setFonts: @convention(c) (UnsafePointer<CChar>?, UnsafePointer<UnsafePointer<CChar>?>?, Int32) -> Void =
                function("vnkk_set_fonts"),
              let start: @convention(c) (UnsafePointer<VNKKLaunch>?) -> Int32 = function("vnkk_start"),
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
              let view: @convention(c) () -> UnsafeMutableRawPointer? = function("vnkk_view"),
              let setMenuBar: @convention(c) (Int32) -> Void = function("vnkk_set_menu_bar"),
              let menuBarShown: @convention(c) () -> Int32 = function("vnkk_menu_bar_shown"),
              let menu: @convention(c) () -> UnsafePointer<CChar>? = function("vnkk_menu"),
              let menuClick: @convention(c) (UnsafePointer<CChar>?) -> Bool = function("vnkk_menu_click")
        else { return nil }
        return KiriKiriEngine(setFonts: setFonts, start: start, iterate: iterate, key: key, mouseButton: mouseButton, mouseMove: mouseMove,
                              click: click, wheel: wheel, text: text, setPaused: setPaused, requestQuit: requestQuit,
                              saveSystemData: saveSystemData, stop: stop, view: view, setMenuBar: setMenuBar,
                              menuBarShown: menuBarShown, menu: menu, menuClick: menuClick)
    }

    /// Hands the engine the Windows font substitutes FontPack has configured, for the next game it starts.
    func useFontPack() {
        guard let table = FontPack.table else { return }
        let folders = FontPack.folders.map { strdup($0.path) }
        defer { folders.forEach { free($0) } }
        folders.map { UnsafePointer($0) }.withUnsafeBufferPointer {
            setFonts(table.path, $0.baseAddress, Int32($0.count))
        }
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
