import Foundation
import VNCore

extension EngineID {
    nonisolated public static let tmrhiro = EngineID("tmrhiro")
}

nonisolated public struct TmrHiroPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .tmrhiro, displayName: "Tmr-Hiro ADV System", shortName: "Tmr-Hiro", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = TmrHiroLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = TmrHiroLayout(FolderProbe(scanning: launch.folder)) else { throw TmrHiroError.notTmrHiro }
        return TmrHiroSession(launch, root: layout.url(in: launch.folder))
    }

    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = TmrHiroLayout(FolderProbe(scanning: folder)) else { return [] }
        let directory = FileCase.resolve("save", in: layout.url(in: folder))
        return [SaveLocation(directory: directory) { path in Self.saveName(path) }]
    }

    public var defaultControls: ControlLayout { .visualNovel }

    nonisolated static func saveName(_ path: String) -> String? {
        guard let name = path.replacingOccurrences(of: "\\", with: "/").split(separator: "/").last.map(String.init) else { return nil }
        let lower = name.lowercased()
        if lower == "vndegen-marks.bin" { return name }
        guard lower.hasPrefix("savedata") else { return nil }
        let rest = lower.dropFirst("savedata".count)
        let digits: Substring
        if rest.hasPrefix(".") {
            digits = rest.dropFirst()
        } else if rest.hasSuffix(".bmp") {
            digits = rest.dropLast(4)
        } else {
            return nil
        }
        return digits.count == 3 && digits.allSatisfy(\.isNumber) ? name : nil
    }
}

nonisolated public enum TmrHiroError: Error, LocalizedError {
    case notTmrHiro

    public var errorDescription: String? {
        switch self {
        case .notTmrHiro: "No Tmr-Hiro ADV System game was found in this folder."
        }
    }
}
