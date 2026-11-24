import SwiftUI
import Testing
@testable import ControllerUI

@Test func onTheClosedDuoUprightTheBarTakesTheCameraColumnBelowTheCamera() {
    let bar = SideBarPlacement(edge: .trailing, size: CGSize(width: 466, height: 678),
                               insets: EdgeInsets(top: 0, leading: 0, bottom: 34, trailing: 84),
                               occlusions: [CGRect(x: 399.67, y: 29.33, width: 37, height: 37), CGRect(x: 382, y: 0, width: 84, height: 82)])
    #expect(bar == SideBarPlacement(edge: .trailing, width: 84, top: 90, bottom: 42))
}

@Test func onItsSideTheBarKeepsAboveACameraAtTheBottomOfItsColumn() {
    let bar = SideBarPlacement(edge: .trailing, size: CGSize(width: 678, height: 466),
                               insets: EdgeInsets(top: 0, leading: 0, bottom: 34, trailing: 84),
                               occlusions: [CGRect(x: 594, y: 384, width: 84, height: 82)])
    #expect(bar == SideBarPlacement(edge: .trailing, width: 84, top: 8, bottom: 90))
    let other = SideBarPlacement(edge: .leading, size: CGSize(width: 678, height: 466),
                                 insets: EdgeInsets(top: 0, leading: 84, bottom: 34, trailing: 0),
                                 occlusions: [CGRect(x: 0, y: 0, width: 84, height: 82)])
    #expect(other == SideBarPlacement(edge: .leading, width: 84, top: 90, bottom: 42))
}

@Test func withoutASafeAreaColumnTheBarTakesItsOwnWidthAndACameraElsewhereIsIgnored() {
    let bar = SideBarPlacement(edge: .trailing, size: CGSize(width: 951, height: 669),
                               insets: EdgeInsets(top: 0, leading: 0, bottom: 34, trailing: 0),
                               occlusions: [CGRect(x: 677, y: 21, width: 58, height: 37)])
    #expect(bar == SideBarPlacement(edge: .trailing, width: 72, widening: 72, top: 8, bottom: 42))
}

@Test func theBarShowsEveryItemThatFitsAndMovesTheRestIntoOneMenuFromTheBottom() {
    #expect(SideBarFit.height([1, 2, 1]) == CGFloat(228))
    #expect(SideBarFit.shown([1, 2, 1], in: 228) == 4)
    #expect(SideBarFit.shown([1, 2, 1], in: 227) == 2)
    #expect(SideBarFit.shown([1, 2, 1], in: 160) == 1)
    #expect(SideBarFit.shown([1, 2, 1], in: 40) == 0)
    #expect(SideBarFit.leading(3, of: [1, 2, 1]) == [1, 2, 0])
}
