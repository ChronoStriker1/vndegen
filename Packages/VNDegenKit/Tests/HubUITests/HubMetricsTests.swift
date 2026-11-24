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
