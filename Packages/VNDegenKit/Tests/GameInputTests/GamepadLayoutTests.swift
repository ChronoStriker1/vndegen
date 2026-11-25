import CoreGraphics
import Testing
@testable import GameInput

private let duoOnItsSide = CGSize(width: 678, height: 432)
private let cameraLeft = [CGRect(x: 0, y: 0, width: 84, height: 82), CGRect(x: 29.33, y: 29.33, width: 37, height: 37)]
private let cameraRight = [CGRect(x: 594, y: 384, width: 84, height: 82), CGRect(x: 611.67, y: 399.67, width: 37, height: 37)]

private func near(_ a: CGRect, _ b: CGRect) -> Bool {
    abs(a.minX - b.minX) < 0.01 && abs(a.minY - b.minY) < 0.01 && abs(a.width - b.width) < 0.01 && abs(a.height - b.height) < 0.01
}

@Test func aFullWidthGamepadOnTheDuoOnItsSideHasTheSameMarginOnBothEdges() {
    let layout = GamepadLayout(size: duoOnItsSide)
    #expect(abs(layout.unit - 69.677) < 0.001)
    #expect(near(layout.l2, CGRect(x: 20.903, y: 106.606, width: 83.613, height: 41.806)))
    #expect(near(layout.r2, CGRect(x: 573.484, y: 106.606, width: 83.613, height: 41.806)))
    #expect(near(layout.l, CGRect(x: 20.903, y: 168.619, width: 83.613, height: 41.806)))
    #expect(near(layout.home, CGRect(x: 118.452, y: 167.923, width: 43.2, height: 43.2)))
    #expect(near(layout.view, CGRect(x: 459.213, y: 167.923, width: 43.2, height: 43.2)))
    #expect(near(layout.menu, CGRect(x: 516.348, y: 167.923, width: 43.2, height: 43.2)))
    #expect(near(layout.r, CGRect(x: 573.484, y: 168.619, width: 83.613, height: 41.806)))
    #expect(near(layout.dpad, CGRect(x: 20.903, y: 240.387, width: 174.194, height: 174.194)))
    #expect(near(layout.face, CGRect(x: 473.148, y: 230.632, width: 183.948, height: 183.948)))
    #expect(abs(layout.dpad.minX - (678 - layout.face.maxX)) < 0.01)
}

@Test func withTheCameraOnTheLeftNoGamepadButtonMoves() {
    let layout = GamepadLayout(size: duoOnItsSide)
    #expect(layout.keepingClear(of: cameraLeft, in: duoOnItsSide) == layout)
}

@Test func withTheCameraOnTheRightOnlyTheFaceButtonsMoveLeftOffTheHousing() {
    let layout = GamepadLayout(size: duoOnItsSide)
    let cleared = layout.keepingClear(of: cameraRight, in: duoOnItsSide)
    #expect(near(cleared.face, CGRect(x: 467.187, y: 230.632, width: 183.948, height: 183.948)))
    var unmoved = cleared
    unmoved.face = layout.face
    #expect(unmoved == layout)
}

@Test func aShortenedGamepadSlidesItsShouldersOffTheCameraWithoutLandingOnAnotherButton() {
    let size = CGSize(width: 678, height: 276)
    let layout = GamepadLayout(size: size)
    #expect(near(layout.l2, CGRect(x: 16.2, y: 23.82, width: 64.8, height: 32.4)))
    #expect(near(layout.l, CGRect(x: 16.2, y: 71.88, width: 64.8, height: 32.4)))
    let cleared = layout.keepingClear(of: cameraLeft, in: size)
    #expect(near(cleared.l2, CGRect(x: 84, y: 23.82, width: 64.8, height: 32.4)))
    #expect(near(cleared.l, CGRect(x: 16.2, y: 82, width: 64.8, height: 32.4)))
    #expect(cleared.home == layout.home && cleared.dpad == layout.dpad && cleared.r2 == layout.r2)
}

@Test func aButtonInTheCameraCornerTakesTheShorterWayOut() {
    let screen = CGRect(origin: .zero, size: duoOnItsSide)
    #expect(offsetClearing([CGRect(x: 10, y: 10, width: 48, height: 48)], of: cameraLeft, within: screen) == CGSize(width: 0, height: 72))
    let below = offsetClearing([CGRect(x: 620, y: 374, width: 48, height: 48)], of: cameraRight, within: screen)
    #expect(below.width == 0 && abs(below.height + 38) < 0.001)
    #expect(offsetClearing([CGRect(x: 10, y: 374, width: 48, height: 48)], of: cameraLeft, within: screen) == .zero)
}
