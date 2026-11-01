import Foundation
import Testing
import VNCore
import WebKit
import WebRuntime
@testable import EngineRPGMaker

/// Stand-ins for the parts of MZ the cheats use, with MZ's method names and behavior, loaded as script files as MZ's are.
/// `mv` swaps MZ's update count for MV's clock.
private func engine(mv: Bool = false) -> String {
    """
    function Game_BattlerBase() {}
    Game_BattlerBase.prototype.isActor = function () { return false; };
    Game_BattlerBase.prototype.setHp = function (hp) { this._hp = Math.min(Math.max(hp, 0), this.mhp); };
    Game_BattlerBase.prototype.setMp = function (mp) { this._mp = Math.min(Math.max(mp, 0), this.mmp); };
    Game_BattlerBase.prototype.paySkillCost = function (skill) { this._mp -= skill.mpCost; };
    Game_BattlerBase.prototype.recoverAll = function () { this._hp = this.mhp; this._mp = this.mmp; };
    Object.defineProperty(Game_BattlerBase.prototype, 'hp', { get() { return this._hp; } });
    Object.defineProperty(Game_BattlerBase.prototype, 'mp', { get() { return this._mp; } });
    function Game_Actor(name) { this._name = name; this.mhp = 100; this.mmp = 30; this._hp = 40; this._mp = 10; }
    Game_Actor.prototype = Object.create(Game_BattlerBase.prototype);
    Game_Actor.prototype.isActor = function () { return true; };
    Game_Actor.prototype.name = function () { return this._name; };
    Game_Actor.prototype.finalExpRate = function () { return 1.5; };
    function Game_Player() {}
    Game_Player.prototype.canEncounter = function () { return true; };
    Game_Player.prototype.isThrough = function () { return false; };
    function Scene_Title() {}
    function Scene_Options() {}
    function Scene_Map() {}
    var SceneManager = {
        run(scene) { this.goto(scene); },
        goto(scene) { this._scene = new scene(); this.onSceneStart(); },
        onSceneStart() {},
        \(mv ? "_getTimeInMsWithoutMobileSafari() { return window.now; }," : "determineRepeatNumber() { return window.repeatCount; },")
    };
    // MZ's application, whose render takes as long as a test asks.
    window.renderMs = 0;
    window.Graphics = { _app: { render() { const end = performance.now() + window.renderMs; while (performance.now() < end); } } };
    window.now = 1000;
    window.repeatCount = 1;
    var $dataSystem = { currencyUnit: 'G', variables: ['', 'Chapter', '', 'Hero name'], switches: ['', 'Met the king', ''] };
    var $dataItems = [null, { id: 1, name: 'Potion' }, { id: 2, name: '' }];
    var $dataWeapons = [null, { id: 1, name: 'Sword' }];
    var $dataArmors = [null];
    var $gameParty = {
        _gold: 50, _items: {}, _actors: [new Game_Actor('Rathi'), new Game_Actor('Mia')],
        gold() { return this._gold; },
        gainGold(n) { this._gold = Math.min(this._gold + n, 99999999); },
        allMembers() { return this._actors; },
        numItems(item) { return this._items[item.name] || 0; },
        gainItem(item, n) { this._items[item.name] = (this._items[item.name] || 0) + n; },
        inBattle() { return false; },
    };
    var $gameVariables = { _data: [0, 3, 0, 'Rathi'], value(id) { return this._data[id] || 0; }, setValue(id, v) { this._data[id] = v; } };
    var $gameSwitches = { _data: [false, true], value(id) { return !!this._data[id]; }, setValue(id, v) { this._data[id] = v; } };
    var $gamePlayer = new Game_Player();
    """
}

/// A page with VNDegen's scripts and the stand-in engine, run the way MV and MZ start their boot scene.
@MainActor private final class TestPage: GamePage {
    let webView: WKWebView
    let template: String
    private let launchScript: String
    private var startScripts: [String]
    private let html: String

    init(settings: CheatSettings, mv: Bool = false) async throws {
        let scripts = URL(filePath: #filePath).deletingLastPathComponent().appending(path: "../../Sources/EngineRPGMaker/Resources")
        template = try String(contentsOf: scripts.appending(path: "rpgmaker-cheats.js").standardized, encoding: .utf8)
        launchScript = try RPGMakerCheats.script(template, settings: settings)
        startScripts = ["window.__vndegen = {};", launchScript]
        let configuration = WKWebViewConfiguration()
        webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 300), configuration: configuration)
        let tags = [engine(mv: mv), "window.started = true;"]
            .map { "<script src=\"data:text/javascript;base64,\(Data($0.utf8).base64EncodedString())\"></script>" }
        html = "<html><body>\(tags.joined())</body></html>"
        try await load()
    }

    /// Loads the page again with the start scripts as they are now, as a reload does.
    func load() async throws {
        let controller = webView.configuration.userContentController
        controller.removeAllUserScripts()
        for script in startScripts {
            controller.addUserScript(WKUserScript(source: script, injectionTime: .atDocumentStart, forMainFrameOnly: true))
        }
        _ = try? await webView.evaluateJavaScript("window.started = false")
        webView.loadHTMLString(html, baseURL: nil)
        let deadline = ContinuousClock.now + .seconds(20)
        while try await webView.evaluateJavaScript("document.readyState === 'complete' && !!window.started") as? Bool != true {
            guard ContinuousClock.now < deadline else { throw CancellationError() }
            try await Task.sleep(for: .milliseconds(50))
        }
    }

    func cheats() -> RPGMakerCheats {
        RPGMakerCheats(page: self, template: template, script: launchScript)
    }

    func replaceStartScript(_ old: String, with new: String) {
        guard let index = startScripts.firstIndex(of: old) else { return }
        startScripts[index] = new
    }

    func callFunction(_ body: String, arguments: [String: Any]) async throws -> Any? {
        try await webView.callAsyncJavaScript(body, arguments: arguments, contentWorld: .page)
    }

    func run(_ javaScript: String) async throws -> Any? {
        try await callFunction("return \(javaScript)", arguments: [:])
    }
}

@Test @MainActor func cheatsWaitForAGameAndReadItsState() async throws {
    let page = try await TestPage(settings: CheatSettings())
    let cheats = page.cheats()
    _ = try await page.run("SceneManager.run(Scene_Title)")
    #expect(try await cheats.state() == CheatState(inGame: false))
    await #expect(throws: RPGMakerCheatError.self) { try await cheats.perform(.addGold(100)) }
    // The title screen's options and load screens hold the title's game objects too.
    _ = try await page.run("SceneManager.goto(Scene_Options)")
    #expect(try await cheats.state().inGame == false)

    _ = try await page.run("SceneManager.goto(Scene_Map)")
    #expect(try await cheats.state().inGame)
    _ = try await page.run("SceneManager.goto(Scene_Options)")
    #expect(try await cheats.state().inGame)
    _ = try await page.run("SceneManager.goto(Scene_Title)")
    #expect(try await cheats.state().inGame == false)
    _ = try await page.run("SceneManager.goto(Scene_Map)")
    let state = try await cheats.state()
    #expect(state.inGame && state.gold == 50 && state.currencyUnit == "G")
    #expect(state.party == [CheatPartyMember(name: "Rathi", hp: 40, maxHP: 100, mp: 10, maxMP: 30),
                            CheatPartyMember(name: "Mia", hp: 40, maxHP: 100, mp: 10, maxMP: 30)])
    // Database entries with no name are blanks in the editor's list.
    #expect(state.items == [CheatItem(kind: .item, number: 1, name: "Potion", owned: 0),
                            CheatItem(kind: .weapon, number: 1, name: "Sword", owned: 0)])
    #expect(state.variables == [CheatVariable(id: 1, name: "Chapter", value: .integer(3)),
                                CheatVariable(id: 2, name: "", value: .integer(0)),
                                CheatVariable(id: 3, name: "Hero name", value: .text("Rathi"))])
    #expect(state.switches == [CheatSwitch(id: 1, name: "Met the king", isOn: true), CheatSwitch(id: 2, name: "", isOn: false)])
}

@Test @MainActor func actionsChangeTheGamesOwnObjects() async throws {
    let page = try await TestPage(settings: CheatSettings())
    let cheats = page.cheats()
    _ = try await page.run("SceneManager.run(Scene_Map)")
    try await cheats.perform(.addGold(1_000))
    try await cheats.perform(.restoreParty)
    try await cheats.perform(.addItem(CheatItem.ID(kind: .weapon, number: 1), count: 10))
    try await cheats.perform(.setVariable(id: 1, .integer(-7)))
    try await cheats.perform(.setVariable(id: 3, .text("Mia")))
    try await cheats.perform(.setSwitch(id: 1, false))
    let state = try await cheats.state()
    #expect(state.gold == 1_050)
    #expect(state.party.allSatisfy { $0.hp == 100 && $0.mp == 30 })
    #expect(state.items.first { $0.name == "Sword" }?.owned == 10)
    #expect(state.variables.map(\.value) == [.integer(-7), .integer(0), .text("Mia")])
    #expect(state.switches.first?.isOn == false)
    // A whole number stays a JavaScript number, as the game's own commands set it.
    #expect(try await page.run("typeof $gameVariables._data[1]") as? String == "number")
}

@Test @MainActor func settingsFromLaunchWrapTheEngineWhenItStarts() async throws {
    var settings = CheatSettings()
    settings.noEncounters = true
    settings.walksThroughWalls = true
    settings.experienceRate = 10
    settings.speed = 4
    let page = try await TestPage(settings: settings)
    #expect(try await page.run("$gamePlayer.canEncounter()") as? Bool == true)
    _ = try await page.run("SceneManager.run(Scene_Map)")
    #expect(try await page.run("$gamePlayer.canEncounter()") as? Bool == false)
    #expect(try await page.run("$gamePlayer.isThrough()") as? Bool == true)
    #expect(try await page.run("$gameParty.allMembers()[0].finalExpRate()") as? Double == 15)
    #expect(try await page.run("SceneManager.determineRepeatNumber(1)") as? Int == 4)
    // A count raised to catch up counts in full after a frame with little work of its own, as on a slow screen, and
    // once after a frame that spent its time on its own work. A frame with no update stays so.
    #expect(try await page.run("(window.repeatCount = 2, Graphics._app.render(), SceneManager.determineRepeatNumber(2))") as? Int == 8)
    #expect(try await page.run("(window.renderMs = 50, Graphics._app.render(), window.renderMs = 0, SceneManager.determineRepeatNumber(2))") as? Int == 4)
    #expect(try await page.run("(window.repeatCount = 0, SceneManager.determineRepeatNumber(1))") as? Int == 0)

    let cheats = page.cheats()
    cheats.apply(CheatSettings())
    try await Task.sleep(for: .milliseconds(200))
    #expect(try await page.run("$gamePlayer.canEncounter()") as? Bool == true)
    #expect(try await page.run("$gamePlayer.isThrough()") as? Bool == false)
    #expect(try await page.run("$gameParty.allMembers()[0].finalExpRate()") as? Double == 1.5)
    // At normal speed MZ's own catch-up is untouched.
    #expect(try await page.run("(window.repeatCount = 2, SceneManager.determineRepeatNumber(1))") as? Int == 2)
}

@Test @MainActor func keepingThePartyHealthyFillsItAndHoldsHPAndMP() async throws {
    let page = try await TestPage(settings: CheatSettings())
    _ = try await page.run("SceneManager.run(Scene_Map)")
    var settings = CheatSettings()
    settings.keepsPartyHealthy = true
    page.cheats().apply(settings)
    try await Task.sleep(for: .milliseconds(200))
    let actor = "$gameParty.allMembers()[0]"
    #expect(try await page.run("[\(actor).hp, \(actor).mp].join()") as? String == "100,30")
    _ = try await page.run("\(actor).setHp(5), \(actor).setMp(0), \(actor).paySkillCost({ mpCost: 12 })")
    #expect(try await page.run("[\(actor).hp, \(actor).mp].join()") as? String == "100,30")
    // Enemies take damage as before.
    #expect(try await page.run("(() => { const enemy = new Game_BattlerBase(); enemy.mhp = 50; enemy.setHp(20); return enemy.hp; })()") as? Int == 20)
}

@Test @MainActor func aPartyStartsFullWhenTheGameStartsWithKeepingItHealthyOn() async throws {
    var settings = CheatSettings()
    settings.keepsPartyHealthy = true
    let page = try await TestPage(settings: settings)
    _ = try await page.run("SceneManager.run(Scene_Title)")
    #expect(try await page.run("$gameParty.allMembers()[0].hp") as? Int == 40)
    // A save loads its own HP and MP, and the map that starts after it fills them.
    _ = try await page.run("SceneManager.goto(Scene_Map)")
    #expect(try await page.run("$gameParty.allMembers().map((a) => a.hp + '/' + a.mp).join()") as? String == "100/30,100/30")
}

@Test @MainActor func aReloadStartsWithTheSettingsAsTheyAreNow() async throws {
    var settings = CheatSettings()
    settings.walksThroughWalls = true
    let page = try await TestPage(settings: settings)
    let cheats = page.cheats()
    settings.walksThroughWalls = false
    settings.noEncounters = true
    cheats.apply(settings)
    try await page.load()
    _ = try await page.run("SceneManager.run(Scene_Map)")
    #expect(try await page.run("$gamePlayer.isThrough()") as? Bool == false)
    #expect(try await page.run("$gamePlayer.canEncounter()") as? Bool == false)
    // Twice, so the script the page starts with is the one the last change made.
    settings.noEncounters = false
    cheats.apply(settings)
    try await page.load()
    _ = try await page.run("SceneManager.run(Scene_Map)")
    #expect(try await page.run("$gamePlayer.canEncounter()") as? Bool == true)
}

@Test @MainActor func cheatsOpenedDuringBootWaitForThePluginsToLoad() async throws {
    let page = try await TestPage(settings: CheatSettings())
    let cheats = page.cheats()
    #expect(try await cheats.state().inGame == false)
    var settings = CheatSettings()
    settings.noEncounters = true
    cheats.apply(settings)
    try await Task.sleep(for: .milliseconds(200))
    // A plugin that loads after that replaces the method outright.
    _ = try await page.run("(Game_Player.prototype.canEncounter = function () { return true; }, true)")
    _ = try await page.run("SceneManager.run(Scene_Map)")
    #expect(try await page.run("$gamePlayer.canEncounter()") as? Bool == false)
}

@Test @MainActor func mvRunsItsClockFasterAtHigherSpeeds() async throws {
    var settings = CheatSettings()
    settings.speed = 2
    let page = try await TestPage(settings: settings, mv: true)
    _ = try await page.run("SceneManager.run(Scene_Map)")
    #expect(try await page.run("SceneManager._getTimeInMsWithoutMobileSafari()") as? Int == 1000)
    _ = try await page.run("window.now = 1100")
    #expect(try await page.run("SceneManager._getTimeInMsWithoutMobileSafari()") as? Int == 1200)
    settings.speed = 1
    page.cheats().apply(settings)
    try await Task.sleep(for: .milliseconds(200))
    // A speed change keeps the clock going on from where it was.
    _ = try await page.run("window.now = 1150")
    #expect(try await page.run("SceneManager._getTimeInMsWithoutMobileSafari()") as? Int == 1250)
}

@Test func scriptSettingsAreJSON() throws {
    var settings = CheatSettings()
    settings.speed = 2
    let script = try RPGMakerCheats.script("let settings = __VNDEGEN_CHEATS__;", settings: settings)
    #expect(script == #"let settings = {"experienceRate":1,"keepsPartyHealthy":false,"noEncounters":false,"speed":2,"walksThroughWalls":false};"#)
}
