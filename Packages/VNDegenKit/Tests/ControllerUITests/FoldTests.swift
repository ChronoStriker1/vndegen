import SwiftUI
import Testing
@testable import ControllerUI

private let bookFold = CGRect(x: 455.5, y: 0, width: 40, height: 669)
private let uprightFold = CGRect(x: 0, y: 455.5, width: 669, height: 40)

@Test func aBookFoldSplitsTheScreenIntoLeftAndRightHalves() throws {
    let split = try #require(FoldSplit(fold: bookFold, in: CGSize(width: 951, height: 635)))
    #expect(split.axis == .vertical)
    #expect(split.leadingOrTop == CGRect(x: 0, y: 0, width: 455.5, height: 635))
    #expect(split.trailingOrBottom == CGRect(x: 495.5, y: 0, width: 455.5, height: 635))
}

@Test func anUprightFoldSplitsTheScreenIntoTopAndBottom() throws {
    let split = try #require(FoldSplit(fold: uprightFold, in: CGSize(width: 669, height: 917)))
    #expect(split.axis == .horizontal)
    #expect(split.leadingOrTop == CGRect(x: 0, y: 0, width: 669, height: 455.5))
    #expect(split.trailingOrBottom == CGRect(x: 0, y: 495.5, width: 669, height: 421.5))
}

@Test func contentKeepsToTheSideItAsksFor() throws {
    let book = try #require(FoldSplit(fold: bookFold, in: CGSize(width: 951, height: 635)))
    #expect(book.insets(vertical: .trailing, horizontal: .bottom) == EdgeInsets(top: 0, leading: 495.5, bottom: 0, trailing: 0))
    #expect(book.insets(vertical: .leading, horizontal: nil) == EdgeInsets(top: 0, leading: 0, bottom: 0, trailing: 495.5))
    #expect(book.insets(vertical: nil, horizontal: .bottom) == EdgeInsets())
    let upright = try #require(FoldSplit(fold: uprightFold, in: CGSize(width: 669, height: 917)))
    #expect(upright.insets(vertical: .trailing, horizontal: .bottom) == EdgeInsets(top: 495.5, leading: 0, bottom: 0, trailing: 0))
    #expect(upright.insets(vertical: .trailing, horizontal: .top) == EdgeInsets(top: 0, leading: 0, bottom: 461.5, trailing: 0))
    #expect(upright.insets(vertical: .leading, horizontal: nil) == EdgeInsets())
}

@Test func aViewThatStartsPartWayAcrossMeasuresTheFoldFromItsOwnEdge() throws {
    let fold = CGRect(x: 417.5, y: 0, width: 40, height: 300)
    let split = try #require(FoldSplit(fold: fold, in: CGSize(width: 875, height: 120)))
    #expect(split.insets(vertical: .leading, horizontal: nil) == EdgeInsets(top: 0, leading: 0, bottom: 0, trailing: 457.5))
}

@Test func aFoldOutsideTheViewOrNoFoldLeavesItAlone() {
    #expect(FoldSplit(fold: bookFold, in: CGSize(width: 440, height: 635)) == nil)
    #expect(FoldSplit(fold: nil, in: CGSize(width: 951, height: 635)) == nil)
}
