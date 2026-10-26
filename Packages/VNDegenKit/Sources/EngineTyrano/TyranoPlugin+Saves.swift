import Foundation
import VNCore

extension TyranoPlugin {
    /// Games with `configSave=file` keep `<projectID>_sf.sav`, `<projectID>_tyrano_data.sav`, and the like at the top of
    /// the game folder, beside the PC executable. Games that save to localStorage keep it in the app's data directory,
    /// where only other VNDegen devices can use it.
    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let game = TyranoGame(folder: folder) else { return [] }
        if game.savesToFiles {
            return [SaveLocation(directory: folder) { Self.saveFileName($0, projectID: game.projectID) }]
        }
        return [SaveLocation(directory: dataDirectory.appendingPathComponent("localStorage", isDirectory: true)) { path in
            let parts = path.split(separator: "/")
            guard parts.count >= 2, parts[parts.count - 2] == "localStorage", parts.last!.hasSuffix(".value") else { return nil }
            return String(parts.last!)
        }]
    }

    /// `C:/Games/Test Game 01/testgame01_sf.sav` maps to `testgame01_sf.sav`. A Mac build run from
    /// a quarantined folder saves the same names to `~/_TyranoGameData/`, which maps the same way.
    static func saveFileName(_ path: String, projectID: String) -> String? {
        guard let name = path.split(separator: "/").last.map(String.init) else { return nil }
        let lower = name.lowercased()
        guard lower.hasPrefix(projectID.lowercased() + "_"), lower.hasSuffix(".sav") else { return nil }
        return name
    }
}
