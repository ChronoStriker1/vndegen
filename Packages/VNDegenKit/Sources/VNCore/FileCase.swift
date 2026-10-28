import Foundation
import Synchronization

/// Finds a game's files whatever case the game names them in, as Windows does. iOS disks and case-sensitive Mac volumes
/// tell `Img/Title.PNG` from `img/title.png`, and Windows games often mix the two.
///
/// The rules match `Engines/Shared/casepath.c`, which the RGSS and KiriKiri engines use: a name that exists as written
/// is itself; otherwise it is the first entry of its folder, in sorted order, that equals it ignoring case. Folder
/// listings are cached and read again when the folder's modification time changes, so the lookup costs one `stat` once
/// a folder has been read, and nothing at all for a name that exists as written.
nonisolated public enum FileCase {
    /// The name of `name` as it is on disk in the folder at `directory`: itself when it exists, otherwise the first entry
    /// that equals it ignoring case. Nil when neither exists.
    public static func entry(_ name: String, in directory: String) -> String? {
        var info = stat()
        if lstat(directory + "/" + name, &info) == 0 { return name }
        return listing(directory)?.first { $0.caseInsensitiveCompare(name) == .orderedSame }
    }

    /// `relative`, a path with forward slashes below `directory`, with each part that exists in another case spelled
    /// as on disk. The parts from the first that matches nothing stay as written, so a new file goes into the right
    /// folder with the game's spelling and a write replaces a file that differs only in case.
    public static func resolve(_ relative: String, in directory: URL) -> URL {
        var path = directory.path
        var parts = relative.split(separator: "/", omittingEmptySubsequences: true).map(String.init)[...]
        while let name = parts.first, let actual = entry(name, in: path) {
            path += "/" + actual
            parts.removeFirst()
        }
        for name in parts { path += "/" + name }
        return URL(fileURLWithPath: path)
    }

    /// `relative` as `resolve` spells it, still relative to `directory`.
    public static func spelling(of relative: String, in directory: URL) -> String {
        let base = directory.path
        let resolved = resolve(relative, in: directory).path
        return resolved.hasPrefix(base + "/") ? String(resolved.dropFirst(base.count + 1)) : relative
    }

    private struct Listing {
        var device: dev_t
        var inode: ino_t
        var modified: timespec
        var names: [String]
    }

    /// Games touch a few hundred folders at most. The cache starts over when it holds more.
    private static let limit = 512
    private static let listings = Mutex<[String: Listing]>([:])

    /// The folder's entries in sorted order, from the cache while the folder is unchanged. Nil when it can't be listed.
    private static func listing(_ directory: String) -> [String]? {
        var info = stat()
        guard stat(directory, &info) == 0, info.st_mode & S_IFMT == S_IFDIR else { return nil }
        let cached = listings.withLock { $0[directory] }
        if let cached, cached.device == info.st_dev, cached.inode == info.st_ino,
           cached.modified.tv_sec == info.st_mtimespec.tv_sec, cached.modified.tv_nsec == info.st_mtimespec.tv_nsec {
            return cached.names
        }
        guard let names = try? FileManager.default.contentsOfDirectory(atPath: directory) else { return nil }
        let listing = Listing(device: info.st_dev, inode: info.st_ino, modified: info.st_mtimespec, names: names.sorted())
        listings.withLock {
            if $0.count >= limit { $0.removeAll(keepingCapacity: true) }
            $0[directory] = listing
        }
        return listing.names
    }
}
