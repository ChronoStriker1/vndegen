import Testing
import VNCore
@testable import EngineKiriKiri

@Test func kiriKiriGamesStartWithTheKeyboardAndMouseLayout() {
    #expect(KiriKiriPlugin().defaultControls == .keyboardAndMouse)
}

@Test func typingReachesKiriKiriAsWindowsSendsIt() {
    var router = KiriKiriInputRouter()
    #expect(router.type(.text("Aoi")) == [.text("Aoi")])
    #expect(router.type(.newline) == [
        .key(scancode: SDLScancode.return, down: true), .text("\r"), .key(scancode: SDLScancode.return, down: false),
    ])
    #expect(router.type(.deleteBackward) == [
        .key(scancode: SDLScancode.backspace, down: true), .text("\u{8}"), .key(scancode: SDLScancode.backspace, down: false),
    ])
    // A Return already held by a button stays down after the newline.
    _ = router.translate(.key(KeyboardKey(key: "Enter", code: "Enter", keyCode: 13), pressed: true))
    #expect(router.type(.newline) == [.text("\r")])
}

@Test func hardwareKeysTypeTheirCharacters() {
    #expect(KiriKiriInput.typed("a") == .text("a"))
    #expect(KiriKiriInput.typed("é") == .text("é"))
    #expect(KiriKiriInput.typed("\r") == .text("\r"))
    #expect(KiriKiriInput.typed("\u{7F}") == .text("\u{8}"))
    // AppKit's arrow keys and other control characters type nothing.
    #expect(KiriKiriInput.typed("\u{F700}") == nil)
    #expect(KiriKiriInput.typed("\u{3}") == nil)
    #expect(KiriKiriInput.typed("") == nil)
}
