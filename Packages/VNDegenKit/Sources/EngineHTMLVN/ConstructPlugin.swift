import Foundation
import OSLog
import VNCore
import WebRuntime

extension EngineID {
    nonisolated public static let construct = EngineID("construct")
}

nonisolated let constructLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "construct")

nonisolated public struct ConstructPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .construct, displayName: "Construct", shortName: "Construct", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let game = ConstructGame(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: game.root.isEmpty ? .certain : .likely, version: "Construct 3", evidence: game.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let game = ConstructGame(folder: launch.folder) else { throw ConstructError.notConstruct }
        guard let url = Bundle.module.url(forResource: "construct", withExtension: "js") else { throw ConstructError.scriptMissing }
        constructLog.notice("starting \(game.evidence, privacy: .public)")
        let configuration = WebGameConfiguration(
            folder: GameFolder(root: game.folder(in: launch.folder), webRoot: game.webRoot), appName: launch.game.title,
            runtime: .browser, userScripts: [try String(contentsOf: url, encoding: .utf8)],
            storageDirectory: WebStorage(dataDirectory: launch.dataDirectory).directory,
            buttonScript: { _, _ in nil }, scriptArgument: "construct-script", log: constructLog
        )
        return try configuration.makeSession(launch)
    }

    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        [WebStorage(dataDirectory: dataDirectory).saveLocation]
    }

    public var defaultControls: ControlLayout { .visualNovel }

    public var defaultControlChanges: [ControlInput: ControlAction] { [.a: .click(.primary)] }

    public var listsEngineFiles: Bool { true }

    public func engineFileList(_ probe: FolderProbe) -> EngineFileList? {
        guard let game = ConstructGame(probe), game.root.isEmpty, game.webRoot == "www", probe.contains(Self.webView2Loader) else { return nil }
        let top = probe.entries.filter { !$0.contains("/") }.sorted()
        let extensions = top.filter { $0.lowercased().hasSuffix(".ext.dll") }
        let executables = top.filter { $0.lowercased().hasSuffix(".exe") }
        let wrapper = executables.count == 1 && (probe.size(executables[0]) ?? .max) < Self.wrapperSize ? executables : []
        return EngineFileList(files: Set([Self.webView2Loader, "WV2ExtManager.dll"] + extensions + wrapper))
    }

    static let webView2Loader = "WebView2Loader.dll"
    static let wrapperSize: Int64 = 10_000_000
}

nonisolated enum ConstructError: LocalizedError {
    case notConstruct
    case scriptMissing

    var errorDescription: String? {
        switch self {
        case .notConstruct: "This folder does not hold a Construct game."
        case .scriptMissing: "The Construct support script is missing from the app bundle."
        }
    }
}
