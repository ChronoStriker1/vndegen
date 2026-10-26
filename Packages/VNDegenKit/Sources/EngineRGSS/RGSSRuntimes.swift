import Foundation
import VNCore

/// The RPG Maker RTPs, the stock graphics, sounds, and fonts games may use without shipping them. Their license lets
/// the player use them to play RPG Maker games but forbids redistributing them, so VNDegen downloads the official
/// installer from Degica's site when the player asks, and unpacks it on the device. docs/rgss-spike.md records the
/// terms and the files.
nonisolated enum RGSSRuntimes {
    static let xp = RuntimePackage(
        id: "rpgmaker-rtp-xp", name: "RPG Maker XP RTP",
        download: URL(string: "https://assets.rpgmakerweb.com/xp_rtp104e.exe")!,
        sha256: "b3bd20ad7f413b40ac233aafd2e061de1dc429c2eadb59d0b3157ba3c47f16b2", size: 22_994_937,
        installer: .innoSetup(path: nil), licensePage: licensePage)

    static let vx = RuntimePackage(
        id: "rpgmaker-rtp-vx", name: "RPG Maker VX RTP",
        download: URL(string: "https://assets.rpgmakerweb.com/vx_rtp102e.zip")!,
        sha256: "8c82c02c876391d9585934454a629748d71b421c4957ada1dff8dc4b013ce403", size: 36_797_892,
        installer: .innoSetup(path: "RPGVX_RTP/Setup.exe"), licensePage: licensePage)

    static let vxAce = RuntimePackage(
        id: "rpgmaker-rtp-vxace", name: "RPG Maker VX Ace RTP",
        download: URL(string: "https://assets.rpgmakerweb.com/RPGVXAce_RTP.zip")!,
        sha256: "7e93d0ead93a686218b7c671bf099ef42f09f536083bd0b2f0fa6423a39fc19b", size: 194_690_591,
        installer: .innoSetup(path: "RTP100/Setup.exe"), licensePage: licensePage)

    static let licensePage = URL(string: "https://www.rpgmakerweb.com/run-time-package")!

    /// The package for a name in `Game.ini`, as RPG Maker's own installers register them. Nil for a name VNDegen
    /// doesn't know, such as a fan-made RTP.
    static func package(named name: String) -> RuntimePackage? {
        switch name.lowercased() {
        case "standard": xp
        case "rpgvx": vx
        case "rpgvxace": vxAce
        default: nil
        }
    }
}
