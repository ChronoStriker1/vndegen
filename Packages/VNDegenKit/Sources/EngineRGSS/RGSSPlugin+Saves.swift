import Foundation
import VNCore

extension RGSSPlugin {
    /// Saves the game writes here go to its data directory, which the engine reads together with the game's folder, so
    /// saves copied in with a PC game still load. RGSS games on PC save beside `Game.exe` as `Save01.rvdata2`,
    /// `Save1.rvdata`, or `Save1.rxdata`.
    nonisolated public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        [SaveLocation(directory: Self.saveDirectory(dataDirectory), relativePath: Self.saveRelativePath)]
    }

    nonisolated static func saveDirectory(_ dataDirectory: URL) -> URL {
        dataDirectory.appendingPathComponent("saves", isDirectory: true)
    }

    /// `C:/Games/Example/Save01.rvdata2` maps to `Save01.rvdata2`. Any file named `Save…` with an RGSS data extension
    /// counts, since games name their slots differently.
    nonisolated static func saveRelativePath(_ path: String) -> String? {
        guard let name = path.split(separator: "/").last.map(String.init) else { return nil }
        let lower = name.lowercased()
        guard lower.hasPrefix("save"), [".rxdata", ".rvdata", ".rvdata2"].contains(where: lower.hasSuffix) else { return nil }
        return name
    }
}
