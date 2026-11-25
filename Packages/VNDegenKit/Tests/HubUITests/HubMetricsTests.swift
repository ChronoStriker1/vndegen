import SwiftUI
import Testing
@testable import HubUI

private func close(_ a: CGFloat, _ b: CGFloat) -> Bool { abs(a - b) < 0.01 }

@MainActor
@Test func theClosedDuoShowsThreeCapsulesAndOneHeaderAcross() {
    let metrics = HubMetrics(size: CGSize(width: 466, height: 644))
    #expect(metrics.gridColumns == 3)
    #expect(close(metrics.gridTileWidth, 134))
    #expect(metrics.headerColumns == 1)
    #expect(close(metrics.headerWidth, 426))
}

@MainActor
@Test func anOddColumnCountGoesEvenWhereTheWindowCanFold() {
    let flat = HubMetrics(size: CGSize(width: 669, height: 917))
    #expect(flat.gridColumns == 5)
    var foldable = flat
    foldable.foldable = true
    #expect(foldable.gridColumns == 6)
    #expect(foldable.headerColumns == 2)
}

@MainActor
@Test func theGapAtAnActiveFoldWidensAndTheOuterMarginsStay() {
    var metrics = HubMetrics(size: CGSize(width: 951, height: 635), foldable: true)
    #expect(metrics.gridColumns == 6)
    #expect(close(metrics.gridTileWidth, 130.82))
    metrics.foldGap = 40
    #expect(metrics.gridColumns == 6)
    #expect(close(metrics.gridTileWidth, 127.15))
    #expect(metrics.columnSpacing(after: 2, of: 6) == 40)
    #expect(metrics.columnSpacing(after: 1, of: 6) == 18)
    #expect(metrics.headerColumns == 2)
    #expect(close(metrics.headerWidth, 417.46))
    #expect(metrics.gridItems(columns: 6, width: 100).map(\.spacing) == [18, 18, 40, 18, 18, 18])
}

@MainActor
@Test func anIPadKeepsItsColumns() {
    let metrics = HubMetrics(size: CGSize(width: 1024, height: 1366))
    #expect(metrics.gridColumns == 6)
    #expect(close(metrics.gridTileWidth, 142.01))
    #expect(metrics.headerColumns == 2)
    #expect(close(metrics.headerWidth, 462.04))
}

@MainActor
@Test func aGamePageBannerFollowsTheWidthBetweenAFloorAndACap() {
    #expect(HubMetrics(size: CGSize(width: 466, height: 644)).bannerHeight == 180)
    #expect(close(HubMetrics(size: CGSize(width: 951, height: 635)).bannerHeight, 307.09))
    #expect(HubMetrics(size: CGSize(width: 1920, height: 1080)).bannerHeight == 540)
    #expect(HubMetrics(size: CGSize(width: 951, height: 300)).bannerHeight == HubMetrics(size: CGSize(width: 951, height: 1200)).bannerHeight)
}

private let duoOnItsSide = CGSize(width: 594, height: 432)

@MainActor
@Test func onTheClosedDuoOnItsSideTheGameTakesTheFullWidthUnlessAskedToKeepClearOfTheCamera() {
    let leading = HubMetrics(size: duoOnItsSide, insets: EdgeInsets(top: 0, leading: 84, bottom: 34, trailing: 0),
                             occlusions: [CGRect(x: 0, y: 0, width: 84, height: 82), CGRect(x: 23.67, y: 29.33, width: 37, height: 37)])
    #expect(leading.gameClearance(.fullWidth) == EdgeInsets())
    #expect(leading.gameClearance(.clearOfCamera) == EdgeInsets(top: 82, leading: 0, bottom: 0, trailing: 0))
    #expect(!leading.cameraAtBottom)

    let trailing = HubMetrics(size: duoOnItsSide, insets: EdgeInsets(top: 0, leading: 0, bottom: 34, trailing: 84),
                              occlusions: [CGRect(x: 594, y: 384, width: 84, height: 82), CGRect(x: 617.33, y: 399.67, width: 37, height: 37)])
    #expect(trailing.gameClearance(.fullWidth) == EdgeInsets())
    #expect(trailing.gameClearance(.clearOfCamera) == EdgeInsets(top: 0, leading: 0, bottom: 82, trailing: 0))
    #expect(trailing.cameraAtBottom)
}

@MainActor
@Test func withoutTheCamerasRegionTheGameKeepsBesideTheCameraColumn() {
    let metrics = HubMetrics(size: duoOnItsSide, insets: EdgeInsets(top: 0, leading: 0, bottom: 34, trailing: 84))
    #expect(metrics.gameClearance(.clearOfCamera) == EdgeInsets(top: 0, leading: 0, bottom: 0, trailing: 84))
}

@MainActor
@Test func theGameClearsNothingWithoutASideCameraHeldSideways() {
    let upright = HubMetrics(size: CGSize(width: 382, height: 644), insets: EdgeInsets(top: 0, leading: 0, bottom: 34, trailing: 84),
                             occlusions: [CGRect(x: 399.67, y: 29.33, width: 37, height: 37), CGRect(x: 382, y: 0, width: 84, height: 82)])
    #expect(upright.gameClearance(.clearOfCamera) == EdgeInsets())
    let iPhone = HubMetrics(size: CGSize(width: 756, height: 381), insets: EdgeInsets(top: 0, leading: 59, bottom: 21, trailing: 59))
    #expect(iPhone.gameClearance(.clearOfCamera) == EdgeInsets())
    let book = HubMetrics(size: CGSize(width: 951, height: 635), insets: EdgeInsets(top: 0, leading: 0, bottom: 34, trailing: 0))
    #expect(book.gameClearance(.clearOfCamera) == EdgeInsets())
}
