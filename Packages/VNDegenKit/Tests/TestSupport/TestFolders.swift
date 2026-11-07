import Foundation
import Synchronization

/// Where tests make their fixture folders. Each test process gets one folder inside the system's temporary folder and
/// deletes it as it exits, so a test run leaves nothing behind there, even where a test doesn't remove what it made.
public enum TestFolders {
    public static let root: URL = makeRoot()

    /// Removes `folder` as the process exits if it is empty then, for a folder the code under test makes outside `root`,
    /// such as one an app run shares. Anything the folder still holds then stays, with the folder.
    public static func removeAtExitIfEmpty(_ folder: URL) {
        _ = root
        foldersToRemoveIfEmpty.withLock { $0.insert(folder.path) }
    }

    private static let foldersToRemoveIfEmpty = Mutex<Set<String>>([])

    private static func makeRoot() -> URL {
        let root = FileManager.default.temporaryDirectory.appending(path: "vndegen-tests-\(UUID().uuidString)", directoryHint: .isDirectory)
        try? FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        atexit {
            try? FileManager.default.removeItem(at: TestFolders.root)
            for path in TestFolders.foldersToRemoveIfEmpty.withLock({ $0 }) { rmdir(path) }
        }
        return root
    }
}
