#if os(macOS)
import AppKit

/// The player's own CrossOver, which VNDegen calls and never ships. CrossOver and CrossOver Preview share one bundle
/// identifier, and the newest installed wins.
nonisolated struct CrossOver: Equatable, Sendable {
    static let bundleID = "com.codeweavers.CrossOver"

    let app: URL
    let version: String

    /// CrossOver's command-line tools.
    var tools: URL { app.appendingPathComponent("Contents/SharedSupport/CrossOver/bin", isDirectory: true) }

    /// The newest installed CrossOver whose `wine` can run. Nil when there is none.
    static func installed() -> CrossOver? {
        let apps = NSWorkspace.shared.urlsForApplications(withBundleIdentifier: bundleID).compactMap { url -> CrossOver? in
            let tools = url.appendingPathComponent("Contents/SharedSupport/CrossOver/bin", isDirectory: true)
            guard FileManager.default.isExecutableFile(atPath: tools.appendingPathComponent("wine").path),
                  FileManager.default.isExecutableFile(atPath: tools.appendingPathComponent("cxbottle").path) else { return nil }
            let version = Bundle(url: url)?.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? "0"
            return CrossOver(app: url, version: version)
        }
        return apps.max { $0.version.compare($1.version, options: .numeric) == .orderedAscending }
    }

    /// CrossOver runs Windows code as x86-64 under Rosetta on Apple silicon. Its runtime is installed here.
    static var hasRosetta: Bool {
        #if arch(arm64)
        FileManager.default.fileExists(atPath: "/Library/Apple/usr/libexec/oah/libRosettaRuntime")
        #else
        true
        #endif
    }
}
#endif
