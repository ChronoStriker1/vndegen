import Foundation
import Testing
import FrameHost

@Suite struct WebLinksTests {
    @Test @MainActor func pagesNeverOpenInTests() {
        #expect(WebLinks.underTest)
        #expect(!WebLinks.mayOpenPages)
    }

    @Test func onlyWebPagesOpen() {
        #expect(WebLinks.page("https://discord.gg/abcdef")?.absoluteString == "https://discord.gg/abcdef")
        #expect(WebLinks.page("HTTP://example.com/a?b=c")?.host == "example.com")
        for text in ["file:///Users/someone/Documents", "/Applications", "C:\\Games\\save", "user://save.dat",
                     "res://icon.png", "steam://run/123", "mailto:a@example.com", "javascript:alert(1)", "https://",
                     "http:example.com", ""] {
            #expect(WebLinks.page(text) == nil, "\(text)")
        }
    }

    @Test func mailOpensOnlyWhereItIsAsked() {
        #expect(WebLinks.page("mailto:someone@example.com") == nil)
        #expect(WebLinks.page("mailto:someone@example.com", mail: true)?.absoluteString == "mailto:someone@example.com")
        #expect(WebLinks.page("https://example.com", mail: true)?.host == "example.com")
        for text in ["mailto:", "file:///etc/hosts", "steam://run/1"] {
            #expect(WebLinks.page(text, mail: true) == nil, "\(text)")
        }
    }
}
