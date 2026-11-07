import Foundation
import TestSupport
import Testing

/// A UserDefaults suite of a test's own, kept in a temporary folder that `remove()` deletes. A suite named rather than
/// given a path lives in ~/Library/Preferences, and its plist stays there even emptied, since the preferences daemon
/// writes it again after the test deletes it.
struct ScratchDefaults {
    let folder: URL
    let defaults: UserDefaults

    init(_ prefix: String) throws {
        folder = TestFolders.root.appending(path: "\(prefix)-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        // An absolute path names the suite's file, without its .plist extension.
        defaults = try #require(UserDefaults(suiteName: folder.appending(path: "defaults").path))
    }

    func remove() {
        try? FileManager.default.removeItem(at: folder)
    }
}
