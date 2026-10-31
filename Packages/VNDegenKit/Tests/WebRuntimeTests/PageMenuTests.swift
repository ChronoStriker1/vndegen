import Testing
import VNCore
@testable import WebRuntime

/// The page's list as Usashitsuke's movie builds it, its full-screen item already left out by the page.
@Test func pageMenuItemsBecomeGameMenuItems() {
    let json = """
    [{"id":"0","title":"タイトルへ戻る","checked":false,"enabled":true,"separatorBefore":true},
     {"id":"1","title":"ゲーム終了","checked":false,"enabled":true,"separatorBefore":false},
     {"id":"3","title":"Sound","checked":true,"enabled":false,"separatorBefore":true}]
    """
    let items = WebPageMenuItem.items(json: json, idPrefix: "page:")
    #expect(items.map(\.id) == ["page:0", "page:1", "page:separator-3", "page:3"])
    #expect(items.map(\.title) == ["タイトルへ戻る", "ゲーム終了", "", "Sound"])
    #expect(items[3].isChecked && !items[3].isEnabled)
    #expect(WebPageMenuItem.items(json: "[]", idPrefix: "page:").isEmpty)
    #expect(WebPageMenuItem.items(json: "undefined", idPrefix: "page:").isEmpty)
}
