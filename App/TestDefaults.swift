#if DEBUG
import CryptoKit
import Foundation
import ObjectiveC

/// With `-libraryRoot`, a DEBUG build keeps its settings apart from the installed app's, which shares its bundle ID and so
/// its preferences. Every setting goes to a suite of its own for that library root: the app's own settings, `@AppStorage`,
/// the engines', and the ones AppKit and UIKit keep for the app, such as the window's frame and the file panel's folder.
/// They all go through `UserDefaults.standard`, so that is what changes, before anything reads a setting. Launch
/// arguments still apply, since a suite reads the argument domain too.
/// Nonisolated, since AppKit reads `standard` from its own queues too.
nonisolated enum TestDefaults {
    fileprivate nonisolated(unsafe) static var suite: UserDefaults?

    /// The suite's name for a library root, the same each run, so a test library keeps its settings between runs. On a
    /// Mac it is the name of the plist in ~/Library/Preferences, which `defaults delete` removes.
    static func suiteName(forLibraryRoot root: String) -> String {
        let path = URL(fileURLWithPath: (root as NSString).expandingTildeInPath).standardizedFileURL.path
        let digest = SHA256.hash(data: Data(path.utf8)).prefix(8).map { String(format: "%02x", $0) }.joined()
        return "com.chronostriker1.vndegen.test-\(digest)"
    }

    /// Runs once, first thing at launch. Without `-libraryRoot` the app keeps the standard defaults.
    static func isolate() {
        guard suite == nil, let root = UserDefaults.standard.string(forKey: "libraryRoot") else { return }
        let name = suiteName(forLibraryRoot: root)
        guard let isolated = UserDefaults(suiteName: name),
              let standard = class_getClassMethod(UserDefaults.self, #selector(getter: UserDefaults.standard)),
              let replacement = class_getClassMethod(UserDefaults.self, #selector(getter: UserDefaults.vndegenTestStandard))
        else { return }
        suite = isolated
        method_exchangeImplementations(standard, replacement)
        NSLog("TESTDEFAULTS suite=%@", name)
    }
}

extension UserDefaults {
    /// What `standard` returns once `TestDefaults.isolate()` has swapped the two.
    @objc nonisolated fileprivate class var vndegenTestStandard: UserDefaults { TestDefaults.suite! }
}
#endif
