import CoreGraphics
import Testing
@testable import HubUI

/// A 3840 by 2160 display with a 31 point menu bar, in AppKit's coordinates, where y grows upward.
private let display = CGRect(x: 0, y: 0, width: 3840, height: 2129)

@Test func aWindowOnScreenStaysWhereItIs() {
    #expect(WindowPlacement.corrected(CGRect(x: 400, y: 300, width: 1280, height: 800), visibleFrames: [display]) == nil)
    // Partly off, with most of it still shown, is the player's choice.
    #expect(WindowPlacement.corrected(CGRect(x: 3000, y: 300, width: 1280, height: 800), visibleFrames: [display]) == nil)
}

@Test func aWindowMostlyOffScreenComesBackOn() {
    // The frame the Mac mini restored: 40 points of a 1280 point window on screen.
    let frame = CGRect(x: 3800, y: 0, width: 1280, height: 2129)
    #expect(WindowPlacement.corrected(frame, visibleFrames: [display]) == CGRect(x: 2560, y: 0, width: 1280, height: 2129))
}

@Test func aWindowLargerThanTheScreenIsCapped() {
    let frame = CGRect(x: 100, y: -200, width: 4000, height: 2500)
    #expect(WindowPlacement.corrected(frame, visibleFrames: [display]) == display)
}

@Test func aWindowOffEveryScreenGoesToTheMainOne() {
    let second = CGRect(x: 3840, y: 0, width: 1920, height: 1050)
    let frame = CGRect(x: -5000, y: 100, width: 800, height: 600)
    #expect(WindowPlacement.corrected(frame, visibleFrames: [display, second]) == CGRect(x: 0, y: 100, width: 800, height: 600))
    // A window across two screens is judged by what all of them show, and capped to the one it overlaps most.
    let across = CGRect(x: 3500, y: 100, width: 1200, height: 1200)
    #expect(WindowPlacement.corrected(across, visibleFrames: [display, second]) == CGRect(x: 3840, y: 0, width: 1200, height: 1050))
}
