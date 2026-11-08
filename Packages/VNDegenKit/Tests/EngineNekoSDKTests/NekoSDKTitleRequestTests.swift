import Testing
@testable import EngineNekoSDK

// A title's web survey or a manual's web link must never open a page from a test: tests don't reach the network. The
// check can't lean on XCTest's environment alone, which isn't always set.
@Suite struct NekoSDKTitleRequestTests {
    @Test @MainActor func pagesNeverOpenInTests() {
        #expect(TitleRequests.underTest)
        #expect(!TitleRequests.mayOpenPages)
    }
}
