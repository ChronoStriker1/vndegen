import Foundation

/// A package of files some games need but don't ship, such as an RPG Maker RTP. VNDegen downloads it from its official
/// source when the player asks, checks it against a pinned SHA-256, unpacks it, and keeps one copy for every game.
public struct RuntimePackage: Hashable, Sendable, Identifiable {
    /// How the downloaded file holds the package's files.
    public enum Installer: Hashable, Sendable {
        /// An Inno Setup installer. `path` is the setup program inside a downloaded zip, or nil when the download is the
        /// setup program itself. Files the installer puts in its `{app}` folder make up the package.
        case innoSetup(path: String?)
    }

    /// Stable, persisted in the install folder's name. Never rename one.
    public let id: String
    /// Shown to the player, such as "RPG Maker VX Ace RTP".
    public let name: String
    /// The official download, over HTTPS.
    public let download: URL
    /// Lowercase hex SHA-256 of the download. Any other file is refused.
    public let sha256: String
    /// The download's size in bytes.
    public let size: Int64
    public let installer: Installer
    /// The publisher's page with the package's license, which the player accepts before it installs.
    public let licensePage: URL
    /// The license as the publisher's page shows it, for installers that carry none of their own.
    public let license: String?

    public init(id: String, name: String, download: URL, sha256: String, size: Int64, installer: Installer, licensePage: URL,
                license: String? = nil) {
        self.id = id
        self.name = name
        self.download = download
        self.sha256 = sha256
        self.size = size
        self.installer = installer
        self.licensePage = licensePage
        self.license = license
    }
}
