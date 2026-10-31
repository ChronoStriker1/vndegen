import Foundation

/// Identifies a games folder the player added. Persisted, and never reused for another folder.
public struct LibraryFolderID: RawRepresentable, Hashable, Codable, Sendable, CustomStringConvertible {
    public let rawValue: String
    public init(rawValue: String) { self.rawValue = rawValue }
    public init(_ rawValue: String) { self.rawValue = rawValue }
    public var description: String { rawValue }
}

/// A games folder the player added beside the built-in one, such as a folder on an external drive, a file server, or
/// elsewhere on the Mac. Like the built-in folder, it holds one subfolder per game.
public struct LibraryFolder: Identifiable, Codable, Hashable, Sendable {
    public let id: LibraryFolderID
    /// Where the folder was when VNDegen last reached it. Settings shows it, and VNDegen looks there when the bookmark
    /// can't be resolved.
    public var url: URL
    /// Finds the folder again after it is moved or renamed. On iPhone and iPad it also carries the permission to read
    /// and write a folder outside the app.
    public var bookmark: Data
    public var addedAt: Date
    /// Folder names of the games found by the last scan that reached the folder, so they stay in the library, shown as
    /// unavailable, while it can't be reached.
    public var lastSeen: [String]
    /// When the player removed the folder. A removed folder is remembered, so adding the same folder again gives it
    /// back its ID and its games their records.
    public var removedAt: Date?

    public init(id: LibraryFolderID = LibraryFolderID(UUID().uuidString), url: URL, bookmark: Data, addedAt: Date = .now,
                lastSeen: [String] = []) {
        self.id = id
        self.url = url
        self.bookmark = bookmark
        self.addedAt = addedAt
        self.lastSeen = lastSeen
    }

    public var name: String { url.lastPathComponent }
}

/// Bookmarks and the security scope of folders outside the app. Each call does file system work, so it runs off the
/// main thread.
nonisolated enum FolderAccess {
    /// A bookmark for `url`. On iPhone and iPad, call it while `url`'s security scope is open.
    static func bookmark(for url: URL) throws -> Data {
        #if canImport(Darwin)
        try url.bookmarkData(options: [], includingResourceValuesForKeys: nil, relativeTo: nil)
        #else
        // Linux, where the package tests run, has no bookmarks. The path stands in for one.
        Data(url.path.utf8)
        #endif
    }

    /// Where `bookmark` points now, and whether it is stale and should be made again. Never asks the player to mount a
    /// volume, so a share that's offline fails rather than prompting.
    static func resolve(_ bookmark: Data) throws -> (url: URL, stale: Bool) {
        #if canImport(Darwin)
        var stale = false
        #if os(macOS)
        let options: URL.BookmarkResolutionOptions = [.withoutUI, .withoutMounting]
        #else
        let options: URL.BookmarkResolutionOptions = [.withoutUI]
        #endif
        let url = try URL(resolvingBookmarkData: bookmark, options: options, relativeTo: nil, bookmarkDataIsStale: &stale)
        return (url, stale)
        #else
        return (URL(fileURLWithPath: String(decoding: bookmark, as: UTF8.self), isDirectory: true), false)
        #endif
    }

    /// Opens `url`'s security scope. True when this call opened one, which `stop` must close. A folder the Mac app
    /// reaches by path, as the app isn't sandboxed, has no scope to open.
    static func start(_ url: URL) -> Bool {
        #if canImport(Darwin)
        url.startAccessingSecurityScopedResource()
        #else
        false
        #endif
    }

    static func stop(_ url: URL) {
        #if canImport(Darwin)
        url.stopAccessingSecurityScopedResource()
        #endif
    }

    /// Where an added folder resolved to, and whether VNDegen opened a security scope there.
    struct Resolved: Sendable {
        let url: URL
        let scoped: Bool
    }

    /// What reaching an added folder found.
    struct Reach: Sendable {
        /// Where the folder is. Nil when it couldn't be reached.
        var url: URL?
        /// True when reaching it opened a security scope at `url`, which replaces any scope opened before.
        var opened = false
        /// The bookmark again, when the old one is stale.
        var refreshed: Data?
        /// Its game folders. Nil when it couldn't be reached.
        var names: [String]?
        var problem: String?
    }

    /// Lists `folder`'s games. The bookmark is resolved on every scan, so a drive plugged back in or a share back online
    /// comes back, and a folder moved away isn't confused with another one made at its old path. Where it resolves to
    /// the place it was reached before, the scope already open there is kept. A folder whose bookmark doesn't resolve
    /// is looked for at its last path.
    static func reach(_ folder: LibraryFolder, current: Resolved?) -> Reach {
        var url = folder.url, stale = false, resolveError: (any Error)?
        do {
            (url, stale) = try resolve(folder.bookmark)
        } catch {
            resolveError = error
        }
        if let current, current.url.standardizedFileURL == url.standardizedFileURL, let names = try? gameFolders(in: current.url) {
            return Reach(url: current.url, refreshed: stale ? try? bookmark(for: url) : nil, names: names)
        }
        let opened = start(url)
        do {
            let names = try gameFolders(in: url)
            // Made inside the scope, which iOS needs to write a bookmark.
            let refreshed = stale ? try? bookmark(for: url) : nil
            return Reach(url: url, opened: opened, refreshed: refreshed, names: names)
        } catch {
            if opened { stop(url) }
            return Reach(problem: problem(resolveError ?? error))
        }
    }

    /// Deletes `folder`, a game's folder directly inside the library folder `root`, to the Trash or for good. Refuses a
    /// link, so a deletion never reaches outside the library folder, and anything that isn't a direct subfolder of `root`.
    static func delete(_ folder: URL, inside root: URL, toTrash: Bool) throws {
        let fm = FileManager.default
        // Not followed: the attributes of a link are the link's own.
        let type = try fm.attributesOfItem(atPath: folder.path)[.type] as? FileAttributeType
        guard type == .typeDirectory else {
            throw LibraryFolderError(message: "\(folder.lastPathComponent) is a link or not a folder, so VNDegen won't delete it. Delete it yourself if you want it gone.")
        }
        let parent = folder.standardizedFileURL.resolvingSymlinksInPath().deletingLastPathComponent()
        guard parent.standardizedFileURL.path == root.standardizedFileURL.resolvingSymlinksInPath().path,
              !["", ".", ".."].contains(folder.lastPathComponent) else {
            throw LibraryFolderError(message: "\(folder.lastPathComponent) isn't a game folder inside its library folder, so VNDegen won't delete it.")
        }
        #if os(macOS)
        if toTrash { return try fm.trashItem(at: folder, resultingItemURL: nil) }
        #endif
        try fm.removeItem(at: folder)
    }

    /// The game folders directly inside `root`: every visible subfolder.
    static func gameFolders(in root: URL) throws -> [String] {
        let fm = FileManager.default
        return try fm.contentsOfDirectory(atPath: root.path).filter { name in
            var isDir: ObjCBool = false
            return !name.hasPrefix(".") && fm.fileExists(atPath: root.appendingPathComponent(name).path, isDirectory: &isDir)
                && isDir.boolValue
        }
    }

    /// Why a folder can't be read, in words for Settings and the game page.
    static func problem(_ error: any Error) -> String {
        let error = error as NSError
        guard error.domain == NSCocoaErrorDomain else { return error.localizedDescription }
        switch error.code {
        case NSFileNoSuchFileError, NSFileReadNoSuchFileError:
            return "The folder isn't there. Connect the drive or share that holds it, then rescan."
        case NSFileReadNoPermissionError:
            return "VNDegen isn't allowed to read the folder. Remove it and add it again."
        default:
            return error.localizedDescription
        }
    }
}
