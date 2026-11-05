import Foundation
import VNCore

extension EngineID {
    nonisolated public static let unity = EngineID("unity")
}

/// Recognizes Windows Unity games. On a Mac, Play opens the game outside VNDegen: the Mac version the game ships, or the
/// Windows version through the player's own CrossOver, in a bottle VNDegen keeps for these games. On iPhone and iPad the
/// game's page says why VNDegen can't run it. docs/unity.md has the study and the option 2 proof behind that.
nonisolated public struct UnityPlugin: EnginePlugin {
    /// Where VNDegen's bottle lives, `<Application Support>/VNDegen/Unity/Bottles`. A DEBUG `-libraryRoot` run keeps it
    /// in that library's support folder instead.
    let bottles: URL

    public init(bottles: URL? = nil) {
        self.bottles = bottles ?? LibraryPaths.standard.support.appendingPathComponent("Unity/Bottles", isDirectory: true)
    }

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .unity, displayName: "Unity", shortName: "Unity", platforms: [.macOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = UnityLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: layout.confidence,
                           version: UnityLayout.version(probe, evidence: layout.evidence), evidence: layout.evidence)
    }

    /// How a game runs on this Mac: its own Mac version when it ships one and the player didn't pick CrossOver, else
    /// the Windows version through CrossOver.
    enum Route: Equatable {
        case macApp(URL)
        case crossOver
    }

    static func route(_ layout: UnityLayout?, folder: URL, dataDirectory: URL) -> Route {
        if let app = layout?.macApp, runWith(dataDirectory: dataDirectory) != .crossOver {
            return .macApp(FileCase.resolve(app, in: folder))
        }
        return .crossOver
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        #if os(macOS)
        guard let layout = UnityLayout(scanning: launch.folder) else { throw UnityError.notFound }
        switch Self.route(layout, folder: launch.folder, dataDirectory: launch.dataDirectory) {
        case let .macApp(app):
            return UnityMacAppSession(launch, app: app)
        case .crossOver:
            guard let crossOver = CrossOver.installed() else { throw UnityError.crossOverMissing }
            guard CrossOver.hasRosetta else { throw UnityError.rosettaMissing }
            guard let exe = layout.exe(in: launch.folder) else { throw UnityError.notFound }
            let bottle = UnityBottle(folder: bottles, tools: crossOver.tools)
            try bottle.checkOwnBottle()
            return UnityCrossOverSession(launch, bottle: bottle, exe: exe,
                                         prefs: registrySave(folder: launch.folder, dataDirectory: launch.dataDirectory))
        }
        #else
        throw UnityError.cantRun
        #endif
    }

    /// The game's `persistentDataPath`: in VNDegen's bottle for the Windows version, in Application Support for the Mac
    /// version. Saves from a PC's backup land there either way.
    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        #if os(macOS)
        let layout = UnityLayout(scanning: folder)
        guard let layout, let info = UnityAppInfo(folder: folder, layout: layout) else { return [] }
        switch Self.route(layout, folder: folder, dataDirectory: dataDirectory) {
        case let .macApp(app):
            // A Mac build keeps `persistentDataPath` under its bundle identifier, as SiNiSistar 2's log shows, and under
            // company and product only without one.
            let support = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support", isDirectory: true)
            let bundle = Self.bundleIdentifier(app)
            let directory = bundle.map { support.appendingPathComponent($0, isDirectory: true) }
                ?? support.appendingPathComponent(info.company, isDirectory: true).appendingPathComponent(info.product, isDirectory: true)
            return [SaveLocation(directory: directory) { info.relativePath($0, macBundle: bundle) }]
        case .crossOver:
            let directory = UnityBottle(folder: bottles, tools: URL(fileURLWithPath: "/")).persistentData(info)
            return [SaveLocation(directory: directory) { info.relativePath($0) }]
        }
        #else
        return []
        #endif
    }

    /// The app's `CFBundleIdentifier`, when it's usable as one folder name.
    static func bundleIdentifier(_ app: URL) -> String? {
        let plist = app.appendingPathComponent("Contents/Info.plist")
        guard let data = try? Data(contentsOf: plist),
              let info = try? PropertyListSerialization.propertyList(from: data, format: nil) as? [String: Any],
              let id = info["CFBundleIdentifier"] as? String, UnityAppInfo.isUsable(id) else { return nil }
        return id
    }

    /// The Windows version's PlayerPrefs, under `HKEY_CURRENT_USER\Software\<company>\<product>` in the bottle, kept in
    /// `PlayerPrefs.reg` in the game's data directory between launches. The Mac version keeps them in a preferences file
    /// that isn't synced.
    public func registrySave(folder: URL, dataDirectory: URL) -> RegistrySave? {
        #if os(macOS)
        let layout = UnityLayout(scanning: folder)
        guard let layout, Self.route(layout, folder: folder, dataDirectory: dataDirectory) == .crossOver,
              let info = UnityAppInfo(folder: folder, layout: layout) else { return nil }
        return RegistrySave(key: info.playerPrefsKey, file: dataDirectory.appendingPathComponent("PlayerPrefs.reg"))
        #else
        return nil
        #endif
    }

    public func compatibilityNote(for game: Game, folder: URL, dataDirectory: URL) -> String? {
        let layout = UnityLayout(scanning: folder)
        let backend = switch layout?.backend {
        case .mono: " This game's code is .NET built for Unity's Windows player (Mono)."
        case .il2cpp: " This game's code is compiled for Windows PCs (IL2CPP)."
        case nil: ""
        }
        #if os(macOS)
        let outside = "It opens in its own window, so VNDegen's menu, controls and cheats don't reach it."
        switch Self.route(layout, folder: folder, dataDirectory: dataDirectory) {
        case let .macApp(app):
            return "This game includes its own Mac version, \(app.lastPathComponent), which Open Mac version starts. \(outside)"
        case .crossOver:
            guard CrossOver.installed() != nil else {
                return "Unity games run on a Mac through CrossOver, which isn't installed. Install it from codeweavers.com, then press Play in CrossOver."
            }
            guard CrossOver.hasRosetta else {
                return "CrossOver runs this game through Rosetta, which isn't installed. Install it with softwareupdate --install-rosetta in Terminal, then press Play in CrossOver."
            }
            let saves = layout.flatMap { UnityAppInfo(folder: folder, layout: $0) } == nil
                ? " Its saves stay on this Mac, because the game doesn't say where it keeps them."
                : " Its saves and PlayerPrefs settings are backed up with Ludusavi like any game's."
            return "Play in CrossOver runs the Windows version through your CrossOver, in a bottle VNDegen keeps for Unity games, "
                + "apart from your own bottles. \(outside) It can reach the network, as on a PC. The first launch sets up the bottle, "
                + "which takes a minute or two." + saves + backend
        }
        #else
        var note = UnityError.cantRun.localizedDescription + backend
        if let app = layout?.macApp {
            let name = app.split(separator: "/").last.map(String.init) ?? app
            note += " It includes its own Mac version, \(name), which runs on a Mac outside VNDegen."
        } else {
            note += " On a Mac, VNDegen can run it through CrossOver."
        }
        return note
        #endif
    }

    public func playLabel(for game: Game, folder: URL, dataDirectory: URL) -> String? {
        #if os(macOS)
        switch Self.route(UnityLayout(scanning: folder), folder: folder, dataDirectory: dataDirectory) {
        case .macApp: "Open Mac version"
        case .crossOver: "Play in CrossOver"
        }
        #else
        nil
        #endif
    }

    // MARK: Variants

    enum RunWith: String, Codable {
        case macApp = "mac"
        case crossOver = "crossover"
    }

    /// Only for a game that ships a Mac version, which can run either way.
    public func variants(for game: Game, folder: URL, dataDirectory: URL) -> EngineVariants? {
        #if os(macOS)
        guard UnityLayout(scanning: folder)?.macApp != nil else { return nil }
        return EngineVariants(title: "Run with", options: [.init(id: RunWith.macApp.rawValue, title: "Its own Mac version"),
                                                            .init(id: RunWith.crossOver.rawValue, title: "CrossOver")],
                              selected: Self.runWith(dataDirectory: dataDirectory)?.rawValue, automatic: "Its own Mac version")
        #else
        nil
        #endif
    }

    public func selectVariant(_ id: String?, for game: Game, dataDirectory: URL) throws {
        let file = Self.settingsFile(dataDirectory)
        guard let choice = id.flatMap(RunWith.init(rawValue:)) else {
            try? FileManager.default.removeItem(at: file)
            return
        }
        try FileManager.default.createDirectory(at: dataDirectory, withIntermediateDirectories: true)
        try JSONEncoder().encode(Settings(runWith: choice)).write(to: file, options: .atomic)
    }

    static func runWith(dataDirectory: URL) -> RunWith? {
        guard let data = try? Data(contentsOf: settingsFile(dataDirectory)) else { return nil }
        return (try? JSONDecoder().decode(Settings.self, from: data))?.runWith
    }

    private struct Settings: Codable {
        var runWith: RunWith?
    }

    private static func settingsFile(_ dataDirectory: URL) -> URL { dataDirectory.appendingPathComponent("unity.json") }
}

nonisolated public enum UnityError: Error, LocalizedError {
    case cantRun
    case crossOverMissing
    case rosettaMissing
    case notFound

    public var errorDescription: String? {
        switch self {
        case .cantRun:
            "VNDegen can't run Unity games on iPhone or iPad. Unity's terms don't let another app include its player, and the game's code and graphics are made for Windows, so it would need a whole new engine."
        case .crossOverMissing:
            "Unity games run on a Mac through CrossOver, which isn't installed. Install it from codeweavers.com, then press Play in CrossOver again."
        case .rosettaMissing:
            "CrossOver needs Rosetta, which isn't installed. Install it with softwareupdate --install-rosetta in Terminal, then try again."
        case .notFound:
            "VNDegen can't find the game's Windows program in its folder."
        }
    }
}
