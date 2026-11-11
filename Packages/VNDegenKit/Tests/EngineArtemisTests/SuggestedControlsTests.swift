import Foundation
import Testing
import VNCore
@testable import EngineArtemis

private func key(_ code: Int) -> ControlAction { .key(KeyboardKey(windowsKeyCode: code)!) }

private let listWindows = """
    advbtn_old={},
    advkey={\r
    def={[1]={btn="CLICK",adv="CLICK",ui="CLICK"},[13]={btn="CLICK",adv="CLICK",ui="CLICK"},[27]={btn="ESC",adv="MWOFF",ui="EXIT"},[37]={btn="LT",adv="QSAVE",ui="LT"},[38]={btn="UP",adv="BACKLOG",ui="UP"},[39]={btn="RT",adv="QLOAD",ui="RT"},[112]={btn="F1",adv="VOICE",ui="F1"},[113]={btn="F2",adv="AUTO",ui="F2"},[114]={btn="F3",adv="SKIP",ui="F3"},[115]={btn="F4",adv="QSAVE",ui="F4"},[117]={btn="F6",adv="SAVE",ui="RB"},[118]={btn="F7",adv="LOAD",ui="L2"},[120]={btn="F9",adv="CONFIG",ui="L3"},[122]={btn="F11",adv="TITLE"},[143]={btn="LGT",adv="AUTO"},[269]={btn="F3",adv="AUTO",ui="F3"},[262]={btn="F7",adv="SAVE",ui="L2"},[17]={btn="CTRL"},},\r
    tbl={CLICK="adv_click",AUTO="adv_auto",},\r
    ctrl="17,140,261",\r
    },
    """

private let variables = """
    keyconfig_keytable = {\r
    \tLOAD\t  = { 1, 114 },\t-- F3\r
    \tSAVE\t  = { 1, 113 },\t-- F2\r
    \tQLOAD\t  = { 0, 120 },\t-- F10\r
    \tQSAVE\t  = { 0, 121 },\t-- F9\r
    \tAUTO\t  = { 0, 116 },\t-- F5\r
    \tSKIP\t  = { 0, 119 },\t-- F8\r
    \tBACKLOG\t  = { 0, 33  },\t-- PageUp\r
    --\tSHORTCUT  = { 0, 229 },\t-- ???\r
    \tCONFIG\t  = { 1, 112 },\t-- F1\r
    \tVOICE\t  = { 0, 115 },\t-- F4\r
    \tDISABLE\t  = nil\r
    }\r
    keyconfig_shortcut_table = {\r
    \t[13] = "LEFTCLICK",\t\t-- ENTER\r
    \t[27] = "WINDOWOFF",\t\t-- ESC\r
    --\t[18] = "CONFIG",\t\t-- ALT\r
    \t[38] = "BACKLOG",\t\t-- up\r
    \t[37] = "DISABLE",\t\t-- left\r
    \t[2]   = "WINDOWOFF",\r
    }\r
    """

@Test func aTblGamesRolesTheDPadDoesntReachGoOnTheFreeButtons() throws {
    let suggestion = try #require(ArtemisControls.suggestion(listWindows: listWindows))
    #expect(suggestion.buttons == [.view: key(113), .l3: key(120), .r3: key(117), .l2: key(118)])
    #expect(suggestion.labels == [.view: "Auto", .l3: "Config", .r3: "Save", .l2: "Load"])
}

@Test func aVarLuaKeyTableIsNotReadSinceItsScriptsLeaveTheShortcutsOff() {
    #expect(ArtemisControls.suggestion(listWindows: variables) == nil)
}

@Test func aButtonWhoseKeyTheGameBindsStays() throws {
    let suggestion = try #require(ArtemisControls.suggestion([
        .init(code: 9, role: "BACKLOG"), .init(code: 16, role: "MWOFF"), .init(code: 116, role: "AUTO"), .init(code: 112, role: "CONFIG"),
    ]))
    #expect(suggestion.buttons == [.r3: key(116), .l2: key(112)])
}

@Test func rolesTheLayoutAlreadyReachesGetNothing() {
    #expect(ArtemisControls.suggestion([
        .init(code: 13, role: "CLICK"), .init(code: 37, role: "QSAVE"), .init(code: 121, role: "QSAVE"), .init(code: 17, role: "AUTO"),
        .init(code: 140, role: "AUTO"),
    ]) == nil)
    #expect(ArtemisControls.suggestion(listWindows: "no keys here") == nil)
}
