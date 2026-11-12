import OSLog
import TestSupport
import Testing
@testable import WebRuntime

@Test @MainActor func onlyAPageThatScrollsTellsTheShimSo() throws {
    for scrolls in [false, true] {
        var game = WebGameConfiguration(folder: GameFolder(root: TestFolders.root, webRoot: ""), appName: "test", buttonScript: { _, _ in nil },
                                        scriptArgument: "test-script", log: Logger())
        game.scrollsPage = scrolls
        let source = try WebGameSession.shimSource(for: game)
        #expect(source.contains(#""scrolling":"page""#) == scrolls, "\(scrolls)")
        #expect(source.contains("if (config.scrolling !== 'page')"))
    }
}
