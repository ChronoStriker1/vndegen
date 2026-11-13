import Testing
@testable import WebRuntime

@Test func onlyThePageOrAClickLeavesForTheBrowser() {
    #expect(WebGameSession.leavesForBrowser(mainFrame: true, clicked: false))
    #expect(WebGameSession.leavesForBrowser(mainFrame: nil, clicked: false))
    #expect(WebGameSession.leavesForBrowser(mainFrame: false, clicked: true))
    #expect(!WebGameSession.leavesForBrowser(mainFrame: false, clicked: false))
}
