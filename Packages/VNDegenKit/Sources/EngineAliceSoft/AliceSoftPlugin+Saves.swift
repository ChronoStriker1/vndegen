import Foundation
import VNCore

extension AliceSoftPlugin {
    /// System 3 games save in their own folder, as on PC, so a PC backup restores where the game reads.
    nonisolated func system3SaveLocations(root: URL) -> [SaveLocation] {
        [SaveLocation(directory: root) { path in Self.system3SaveName(path) }]
    }

    /// The file name when `path` is a System 3 save: `RanceSA.ASD` to `RanceSZ.ASD` (System 3.9), `ASLEEP_A.DAT` and
    /// `ASLEEP.DAT` (System 3.0), and the `<prefix>FS.ALD` file of game-wide flags Toushin Toshi II writes with QE.
    nonisolated static func system3SaveName(_ path: String) -> String? {
        guard let name = path.split(separator: "/").last.map(String.init) else { return nil }
        let lower = name.lowercased()
        if lower.hasSuffix(".asd"), lower.count >= 7 {
            let stem = lower.dropLast(4)
            let letter = stem.last!, s = stem.dropLast().last!
            if s == "s", letter.isLetter, letter.isASCII { return name }
        }
        if lower == "asleep.dat" { return name }
        if lower.hasPrefix("asleep_"), lower.hasSuffix(".dat"), lower.count == 12 { return name }
        if lower.hasSuffix("fs.ald") { return name }
        return nil
    }
}
