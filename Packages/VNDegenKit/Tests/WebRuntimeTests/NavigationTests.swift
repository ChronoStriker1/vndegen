import Testing
import WebKit
@testable import WebRuntime

@MainActor private final class Frame: WKFrameInfo {
    private let main: Bool
    init(main: Bool) { self.main = main; super.init() }
    override var isMainFrame: Bool { main }
}

@MainActor private final class Action: WKNavigationAction {
    private let frame: WKFrameInfo?, type: WKNavigationType
    init(frame: WKFrameInfo?, type: WKNavigationType) { self.frame = frame; self.type = type; super.init() }
    override var targetFrame: WKFrameInfo? { frame }
    override var navigationType: WKNavigationType { type }
}

@MainActor @Test func onlyThePageOrAClickLeavesForTheBrowser() {
    #expect(WebGameSession.leavesForBrowser(Action(frame: Frame(main: true), type: .other)))
    #expect(WebGameSession.leavesForBrowser(Action(frame: nil, type: .other)))
    #expect(WebGameSession.leavesForBrowser(Action(frame: Frame(main: false), type: .linkActivated)))
    #expect(!WebGameSession.leavesForBrowser(Action(frame: Frame(main: false), type: .other)))
}
