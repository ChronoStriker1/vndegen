import Testing
@testable import WebRuntime

@MainActor @Test func aTestRunInTheBackgroundCanGiveThePageFocus() async throws {
    for alwaysFocused in [false, true] {
        let page = try PageHarness(alwaysFocused: alwaysFocused) { _ in .value(.null) }
        try await page.load("<html><body></body></html>")
        let focused = try await page.run("return document.hasFocus();") as? Bool
        #expect(focused == alwaysFocused, "\(alwaysFocused)")
    }
}
