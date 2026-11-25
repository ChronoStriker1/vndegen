import Foundation
import Testing
import WebKit
@testable import WebRuntime

@MainActor private final class PictureMessages: NSObject, WKScriptMessageHandler {
    var sizes: [[Double]] = []

    func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
        if let body = message.body as? [String: Any], let picture = body["picture"] as? [Double] { sizes.append(picture) }
    }

    func next(after count: Int) async throws -> [Double]? {
        for _ in 0..<40 where sizes.count <= count {
            try await Task.sleep(for: .milliseconds(100))
        }
        return sizes.count > count ? sizes.last : nil
    }
}

@MainActor @Test func thePictureIsTheLargestCanvasUnlessThePageNamesItsOwn() async throws {
    let page = try PageHarness(scripts: [WebGameSession.pictureScript]) { _ in .value(.null) }
    let messages = PictureMessages()
    page.webView.configuration.userContentController.add(messages, name: WebGameSession.messageName)
    try await page.load("""
        <html><body><canvas width="200" height="100"></canvas><canvas width="816" height="624"></canvas></body></html>
        """)
    #expect(try await messages.next(after: 0) == [816, 624])
    try await page.run("window.__vndegenPicture = () => [600, 600];")
    #expect(try await messages.next(after: 1) == [600, 600])
    try await page.run("window.__vndegenPicture = () => null;")
    #expect(try await messages.next(after: 2) == [816, 624])
}
