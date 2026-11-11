import Foundation
import TestSupport
import Testing
import VNCore
@testable import GameInput

private struct VisualNovelEngine: EnginePlugin {
    var descriptor: EngineDescriptor {
        EngineDescriptor(id: EngineID("vn"), displayName: "VN", shortName: "VN", platforms: Set(Platform.allCases))
    }

    func detect(_ probe: FolderProbe) -> EngineMatch? { nil }
    @MainActor func makeSession(_ launch: GameLaunch) throws -> any EngineSession { throw CancellationError() }
    var defaultControls: ControlLayout { .visualNovel }
}

@Test func visualNovelIsTheAgreedLayout() {
    let expected: [ControlInput: ControlAction] = [
        .a: .key(.enter), .b: .click(.secondary), .x: .key(.control), .y: .key(.escape),
        .l: .wheel(.up), .r: .wheel(.down), .l2: .click(.secondary), .r2: .click(.primary),
        .view: .key(.tab), .menu: .key(.escape), .l3: .key(.shift), .r3: .click(.primary),
        .up: .key(.arrowUp), .down: .key(.arrowDown), .left: .key(.arrowLeft), .right: .key(.arrowRight),
        .leftStickUp: .key(.arrowUp), .leftStickDown: .key(.arrowDown),
        .leftStickLeft: .key(.arrowLeft), .leftStickRight: .key(.arrowRight),
        .rightStickUp: .mouseMove(.up), .rightStickDown: .mouseMove(.down),
        .rightStickLeft: .mouseMove(.left), .rightStickRight: .mouseMove(.right),
    ]
    #expect(ButtonMapping.visualNovel.overrides == expected)
    #expect(ButtonMapping.visualNovel.action(for: .x) == .key(KeyboardKey(key: "Control", code: "ControlLeft", keyCode: 17)))
    for case let .key(sent) in expected.values { #expect(KeyboardKey.choices.contains(sent)) }
    #expect(GamepadElement.home.action(in: .visualNovel) == .button(.hub))
    #expect(ControlLayout.visualNovel.mapping == .visualNovel)
    #expect(ControllerLayout.builtIns.map(\.id) == [.visualNovel, .keyboardAndMouse, .mouse, .rpgMaker, .gamepad])
    #expect(ControllerLayout.builtIns.first?.name == "Visual novel")
}

@Test func rpgMakerControlsPutRGSSsXYAndZOnTheFreeButtons() {
    let mapping = ButtonMapping.rpgMaker
    let keys: [ControlInput: KeyboardKey] = [
        .a: .enter, .b: .escape, .x: .shift, .y: .x, .menu: .x, .l: .q, .r: .w,
        .l2: .letter("A"), .r2: .letter("S"), .view: .letter("D"),
        .up: .arrowUp, .down: .arrowDown, .left: .arrowLeft, .right: .arrowRight,
        .leftStickUp: .arrowUp, .leftStickDown: .arrowDown, .leftStickLeft: .arrowLeft, .leftStickRight: .arrowRight,
    ]
    #expect(mapping.overrides == keys.mapValues(ControlAction.key))
    for input in [ControlInput.l3, .r3] + Stick.right.directions { #expect(mapping.action(for: input) == .gamepad(input)) }
    #expect(ControlLayout.rpgMaker.mapping == .rpgMaker)
    #expect(ControlLayout.mouse.mapping == .mouse)
}

@Test @MainActor func gamesOnTheOldDefaultMoveAndPlayersButtonsStay() async throws {
    let root = TestFolders.root.appending(path: "vndegen-defaults-\(UUID().uuidString)")
    let paths = LibraryPaths(gamesRoot: root.appending(path: "Games"), support: root.appending(path: "Support"))
    let names = ["Follows", "PickedKeyboardAndMouse", "Changed", "PickedRPGMaker", "Xbox"]
    for name in names {
        try FileManager.default.createDirectory(at: paths.gamesRoot.appending(path: name), withIntermediateDirectories: true)
    }
    let library = LibraryStore(paths: paths, engines: [VisualNovelEngine()])
    await library.refresh()
    var changed = ButtonMapping.keyboardAndMouse
    changed.set(.key(.enter), for: .a)
    var oldRPGMaker = ButtonMapping.rpgMaker
    for input in [ControlInput.l2, .r2, .view] { oldRPGMaker.set(.gamepad(input), for: input) }
    let stored: [String: [ControlInput: ControlAction]?] = [
        "Follows": nil, "PickedKeyboardAndMouse": ButtonMapping.keyboardAndMouse.overrides,
        "Changed": changed.overrides, "PickedRPGMaker": oldRPGMaker.overrides, "Xbox": [:],
    ]
    for name in names {
        library.update(GameID(name)) {
            $0.engineOverride = EngineID("vn")
            $0.buttonMapping = stored[name] ?? nil
        }
    }
    let layouts = LayoutLibrary(paths: paths)
    let expected: [(String, ButtonMapping, ControllerLayout.ID?)] = [
        ("Follows", .visualNovel, .visualNovel),
        ("PickedKeyboardAndMouse", .keyboardAndMouse, .keyboardAndMouse),
        ("Changed", changed, nil),
        ("PickedRPGMaker", oldRPGMaker, nil),
        ("Xbox", ButtonMapping(), .gamepad),
    ]
    for (name, mapping, shown) in expected {
        let game = try #require(library.game(GameID(name)))
        let plays = layouts.mapping(for: game, in: library)
        #expect(plays == mapping, "\(name)")
        #expect(layouts.shown(for: plays, game: game, in: library)?.id == shown, "\(name)")
        #expect(layouts.defaultLayout(for: game, in: library) == .visualNovel, "\(name)")
    }
}
