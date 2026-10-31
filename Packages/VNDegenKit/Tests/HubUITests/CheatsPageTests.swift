import Testing
import VNCore
@testable import HubUI

@MainActor private final class FakeCheats: CheatSession {
    let features: CheatFeatures
    let variableRange = -999...999
    var current: CheatState
    var performed: [CheatAction] = []
    var applied: [CheatSettings] = []

    init(features: CheatFeatures = [.gold, .restoreParty, .items, .keepPartyHealthy, .noEncounters, .walkThroughWalls, .speed,
                                    .experience, .variables, .switches],
         state: CheatState = CheatState(
            inGame: true, gold: 10, currencyUnit: "G",
            items: [CheatItem(kind: .item, number: 1, name: "Potion", owned: 2), CheatItem(kind: .weapon, number: 1, name: "Sword", owned: 0)],
            variables: [CheatVariable(id: 1, name: "Chapter", value: .integer(3)), CheatVariable(id: 12, name: "Gold stash", value: .integer(0)),
                        CheatVariable(id: 13, name: "Party", value: .other("[1,2]"))],
            switches: [CheatSwitch(id: 1, name: "Met the king", isOn: false)])) {
        self.features = features
        current = state
    }

    func state() async throws -> CheatState { current }
    func perform(_ action: CheatAction) async throws { performed.append(action) }
    func apply(_ settings: CheatSettings) { applied.append(settings) }
}

@MainActor private func page(_ cheats: FakeCheats, saved: @escaping (CheatSettings) -> Void = { _ in }) async -> CheatsPage {
    let page = CheatsPage(cheats: cheats, settings: CheatSettings(), saveSettings: saved)
    await page.load()
    return page
}

/// Lets the page's change task finish.
@MainActor private func settle() async {
    for _ in 0..<10 { await Task.yield() }
}

@MainActor @Test func settingsThatStayOnAreSavedAndSentToTheGame() async {
    let cheats = FakeCheats()
    var saved: [CheatSettings] = []
    let page = await page(cheats) { saved.append($0) }
    #expect(page.tabs == [.quick, .items, .variables, .switches])
    // The first press shows the ring on the tabs; down reaches the gold row.
    _ = page.handle(.down)
    #expect(page.isFocused(.tab(.quick)))
    _ = page.handle(.down)
    #expect(page.isFocused(.gold(1_000)))
    _ = page.handle(.down)
    _ = page.handle(.down)
    #expect(page.isFocused(.keepHealthy))
    _ = page.handle(.confirm)
    #expect(page.settings.keepsPartyHealthy && saved.last == page.settings && cheats.applied.last == page.settings)
    for _ in 0..<3 { _ = page.handle(.down) }
    #expect(page.isFocused(.speed))
    // Left and right step through the speeds and stop at the ends.
    for _ in 0..<3 { _ = page.handle(.right) }
    #expect(page.settings.speed == 4 && page.isFocused(.speed))
    _ = page.handle(.left)
    #expect(page.settings.speed == 2)
    #expect(page.handle(.cancel) == false)
}

@MainActor @Test func goldAndItemsGoToTheGame() async {
    let cheats = FakeCheats()
    let page = await page(cheats)
    page.select(.gold(10_000))
    await settle()
    page.select(.item(CheatItem.ID(kind: .item, number: 1), count: 10))
    await settle()
    #expect(cheats.performed == [.addGold(10_000), .addItem(CheatItem.ID(kind: .item, number: 1), count: 10)])
}

@MainActor @Test func tabsSwitchWithTheShouldersAndFilterTheirLists() async {
    let page = await page(FakeCheats())
    _ = page.handle(.pageDown)
    #expect(page.tab == .items && page.shownItems.map(\.name) == ["Potion"])
    page.select(.itemKind(.weapon))
    #expect(page.shownItems.map(\.name) == ["Sword"])
    _ = page.handle(.pageDown)
    #expect(page.tab == .variables && page.shownVariables.count == 3)
    page.query = "gold"
    #expect(page.shownVariables.map(\.id) == [12])
    page.query = "#1"
    #expect(page.shownVariables.map(\.id) == [1])
    page.query = "13"
    #expect(page.shownVariables.map(\.id) == [13])
    // Back around to the first tab, with the search cleared.
    _ = page.handle(.pageDown)
    _ = page.handle(.pageDown)
    #expect(page.tab == .quick && page.query.isEmpty)
}

@MainActor @Test func variablesStepAndSwitchesToggleInPlace() async {
    let cheats = FakeCheats()
    let page = await page(cheats)
    page.show(.variables)
    _ = page.handle(.down)
    _ = page.handle(.down)
    _ = page.handle(.down)
    #expect(page.isFocused(.variable(1)))
    _ = page.handle(.right)
    await settle()
    #expect(page.variable(1)?.value == .integer(4))
    // A value the page doesn't edit lets left and right move on as usual.
    page.select(.variable(13))
    #expect(page.keyboard == nil)
    page.show(.switches)
    page.select(.toggle(1))
    await settle()
    #expect(page.state?.switches.first?.isOn == true)
    #expect(cheats.performed == [.setVariable(id: 1, .integer(4)), .setSwitch(id: 1, true)])
}

@MainActor @Test func editingAVariableSendsItWhenThePlayerFinishes() async throws {
    let cheats = FakeCheats()
    let page = await page(cheats)
    page.show(.variables)
    page.select(.variable(12))
    let keyboard = try #require(page.keyboard)
    keyboard.replace(with: "250")
    #expect(cheats.performed.isEmpty)
    keyboard.press(.done)
    await settle()
    #expect(page.keyboard == nil && page.variable(12)?.value == .integer(250))
    for typed in ["lots", "1000"] {
        page.select(.variable(12))
        page.keyboard?.replace(with: typed)
        page.keyboard?.press(.done)
        await settle()
        #expect(page.status?.isError == true && cheats.performed.count == 1)
    }
}

@MainActor @Test func steppingStopsAtTheEndsOfWhatAVariableHolds() async {
    let cheats = FakeCheats(state: CheatState(inGame: true, variables: [CheatVariable(id: 1, name: "", value: .integer(999))]))
    let page = await page(cheats)
    page.show(.variables)
    for _ in 0..<3 { _ = page.handle(.down) }
    #expect(page.isFocused(.variable(1)))
    _ = page.handle(.right)
    await settle()
    #expect(cheats.performed.isEmpty)
    _ = page.handle(.left)
    await settle()
    #expect(cheats.performed == [.setVariable(id: 1, .integer(998))])
}

@MainActor @Test func onTheTitleScreenOnlyTheSettingsShow() async {
    let page = await page(FakeCheats(state: CheatState(inGame: false)))
    #expect(page.rows == [[.tab(.quick), .tab(.items), .tab(.variables), .tab(.switches)], [.keepHealthy], [.noEncounters],
                          [.walkThrough], [.speed], [.experience]])
    page.show(.variables)
    #expect(page.rows.count == 1)
}

@MainActor @Test func theCheatsEntryShowsOnlyForGamesThatHaveCheats() {
    let menu = PlayerMenu()
    #expect(!menu.items(for: ControlMethod.automatic).contains(.cheats))
    menu.offersCheats = true
    #expect(Array(menu.items(for: ControlMethod.automatic).prefix(3)) == [.resume, .cheats, .quit])
    menu.page = .cheats
    menu.closeCheats()
    #expect(menu.page == .main && menu.focus == .cheats)
}
