import CoreGraphics
import Testing
import VNCore
@testable import HubUI

@MainActor @Test func aReleasedMenuButtonSnapsToTheCornerOfItsQuarter() {
    let size = CGSize(width: 1000, height: 800)
    #expect(MenuButtonCorner.nearest(to: CGPoint(x: 120, y: 90), in: size) == .topLeading)
    #expect(MenuButtonCorner.nearest(to: CGPoint(x: 880, y: 90), in: size) == .topTrailing)
    #expect(MenuButtonCorner.nearest(to: CGPoint(x: 120, y: 700), in: size) == .bottomLeading)
    #expect(MenuButtonCorner.nearest(to: CGPoint(x: 880, y: 700), in: size) == .bottomTrailing)
    #expect(MenuButtonCorner.nearest(to: CGPoint(x: 499, y: 401), in: size) == .bottomLeading)
    #expect(MenuButtonCorner.nearest(to: CGPoint(x: 1400, y: -300), in: size) == .topTrailing)
    #expect(MenuButtonCorner.nearest(to: CGPoint(x: -60, y: 1200), in: size) == .bottomLeading)
}

@MainActor @Test func theMenuButtonStartsTopRightAndVoiceOverStepsItRoundTheCorners() {
    #expect(MenuButtonCorner.standard == .topTrailing)
    #expect(MenuButtonCorner.topTrailing.next == .bottomTrailing)
    #expect(MenuButtonCorner.bottomTrailing.next == .bottomLeading)
    #expect(MenuButtonCorner.bottomLeading.next == .topLeading)
    #expect(MenuButtonCorner.topLeading.next == .topTrailing)
}
