import Foundation
import Testing
import VNCore
@testable import EngineWolfRPG

private func key(_ code: Int) -> ControlAction { .key(KeyboardKey(windowsKeyCode: code)!) }

@Test func namedKeysComeFirstAndTheRestFollowInTheGamesOrder() throws {
    let checks = WolfRPGControls.checks("130\tダッシュ\n131\tダッシュ\n116\tメッセージウィンドウ消去\n130\tダッシュ\n117\tメッセージ簡易ログ\n132\tダッシュ\n")
    let suggestion = try #require(WolfRPGControls.suggestion(checks))
    #expect(suggestion.buttons == [.view: key(81), .l2: key(87), .l3: key(65), .r3: key(83)])
    #expect(suggestion.labels == [.view: "Hide window", .l2: "Message log"])
}

@Test func keysAButtonSendsDebugEventsAndKeysNoButtonCanSendAreSkipped() {
    let checks = WolfRPGControls.checks("129\t文章スキップ\n142\tボタン監視\n157\tデバッグ起動用\n148\tﾁｰﾄﾎﾞﾀﾝ\n159\tダッシュ\n257\tメッセージスキップ\n300\t\n")
    #expect(WolfRPGControls.suggestion(checks) == nil)
}

@Test func aNameLabelsAKeyOnlyOnAnExactMatch() throws {
    let checks = WolfRPGControls.checks("148\t--バックログ描画\n148\tバックログ表示\n147\tバックログ表示2\n115\t\n")
    let suggestion = try #require(WolfRPGControls.suggestion(checks))
    #expect(suggestion.buttons == [.view: key(66), .l2: key(86), .l3: key(9)])
    #expect(suggestion.labels == [.view: "Backlog"])
}

@Test func wolfKeyCodesBecomeTheKeysTheRouterSends() {
    #expect(WolfRPGControls.key(wolfCode: 116) == KeyboardKey(windowsKeyCode: 81))
    #expect(WolfRPGControls.key(wolfCode: 138) == KeyboardKey(windowsKeyCode: 76))
    #expect(WolfRPGControls.key(wolfCode: 150) == KeyboardKey(windowsKeyCode: 77))
    #expect(WolfRPGControls.key(wolfCode: 102) == KeyboardKey(windowsKeyCode: 49))
    #expect(WolfRPGControls.key(wolfCode: 157) == KeyboardKey(windowsKeyCode: 32))
    #expect(WolfRPGControls.key(wolfCode: 159) == nil)
    #expect(WolfRPGControls.key(wolfCode: 139) == nil)
    for code in [116, 130, 144, 102, 111, 115, 157] {
        let windows = WolfRPGControls.key(wolfCode: code)!.keyCode
        #expect(WolfRPGInputRouter.wolfKey(windowsKeyCode: windows) != nil)
    }
}
