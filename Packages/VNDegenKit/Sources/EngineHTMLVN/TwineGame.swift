import Foundation
import VNCore
import WebRuntime

/// A Twine story in the folder the user copied in: the HTML file that holds it and where it is served from. Read from
/// the folder for detection and for each launch.
nonisolated struct TwineGame: Equatable, Sendable {
    /// The folder the page is served from, relative to the copied folder: the folder holding the story's file, or the
    /// `browser_assets` pack of a Unity player built with Zen Fulcrum's Embedded Browser.
    let webRoot: String
    /// The story's file in the web root, such as `index.html`.
    let page: String
    /// The story format and its version, such as "Harlowe 2.1.0", or "Twine 1" for a Twine 1 story.
    let format: String?

    /// The path of the story's file, or of the pack holding it, relative to the copied folder.
    var evidence: String {
        if Self.isBrowserAssets(webRoot) { return webRoot }
        return webRoot.isEmpty ? page : "\(webRoot)/\(page)"
    }

    /// How far into a file detection looks for the story. Story formats put their own code first, about 240 KB of it for
    /// SugarCube 2.
    static let scanLimit = 4 << 20
    /// The most HTML files detection reads in one folder.
    static let maxPages = 16
}

nonisolated extension TwineGame {
    /// The shallowest story in the folder's listing, checking `index.html` before other files at the same depth, then
    /// the Embedded Browser packs.
    init?(_ probe: FolderProbe) {
        let pages = probe.entries.filter { Self.isCandidate($0) }
            .sorted { Self.order($0) < Self.order($1) }
            .prefix(Self.maxPages)
        for path in pages {
            guard let format = Self.storyFormat(scanning: probe.root.appendingPathComponent(path)) else { continue }
            let folder = (path as NSString).deletingLastPathComponent
            self.init(webRoot: folder, page: (path as NSString).lastPathComponent, format: format.isEmpty ? nil : format)
            return
        }
        for pack in probe.entries.filter(Self.isBrowserAssets).sorted() {
            let web = GameFolder(root: probe.root, webRoot: pack)
            guard let data = web.read(servedPath: "/index.html", limit: Self.scanLimit),
                  let format = Self.storyFormat(in: data) else { continue }
            self.init(webRoot: pack, page: "index.html", format: format.isEmpty ? nil : format)
            return
        }
        return nil
    }

    init?(folder: URL) {
        self.init(FolderProbe(scanning: folder))
    }

    /// An HTML file that could hold a story. Chromium's license pages, which NW.js and Electron builds ship, are large
    /// and never do.
    private static func isCandidate(_ path: String) -> Bool {
        let name = (path as NSString).lastPathComponent.lowercased()
        guard name.hasSuffix(".html") || name.hasSuffix(".htm") else { return false }
        return !["credits.html", "licenses.chromium.html"].contains(name)
    }

    private static func order(_ path: String) -> (Int, Int, String) {
        let name = (path as NSString).lastPathComponent.lowercased()
        return (path.split(separator: "/").count, name == "index.html" ? 0 : 1, path)
    }

    /// `<Game>_Data/Resources/browser_assets`, where Embedded Browser keeps a Unity game's pages.
    static func isBrowserAssets(_ path: String) -> Bool {
        let parts = path.lowercased().split(separator: "/")
        return parts.count >= 3 && parts[parts.count - 1] == Substring(BrowserAssetsPack.fileName)
            && parts[parts.count - 2] == "resources" && parts[parts.count - 3].hasSuffix("_data")
    }

    /// Reads `url` a chunk at a time, up to `scanLimit`, until it finds a story.
    static func storyFormat(scanning url: URL) -> String? {
        guard let handle = try? FileHandle(forReadingFrom: url) else { return nil }
        defer { try? handle.close() }
        var data = Data()
        var searched = 0
        while data.count < scanLimit, let chunk = try? handle.read(upToCount: 256 << 10), !chunk.isEmpty {
            data.append(chunk)
            if let format = storyFormat(in: data, from: searched) { return format }
            // The next search starts far enough back to find a marker, or a whole story tag, that this read cut off.
            searched = max(0, data.count - (64 << 10))
        }
        return nil
    }

    /// The story format named by the story data in `data`, looking from `start` on: "Harlowe 2.1.0", "Twine 1", or empty
    /// when a Twine 2 story names none. Nil when `data` holds no story data, or holds only the start of its tag.
    static func storyFormat(in data: Data, from start: Int = 0) -> String? {
        let range = start..<data.count
        if data.range(of: twine1Marker, in: range) != nil { return "Twine 1" }
        guard let start = data.range(of: twine2Marker, in: range),
              let end = data[start.upperBound...].firstIndex(of: UInt8(ascii: ">")) else { return nil }
        let tag = String(decoding: data[start.lowerBound..<end], as: UTF8.self)
        let name = tag.firstMatch(of: /\sformat\s*=\s*"([^"]*)"/).map { String($0.1) }
        let version = tag.firstMatch(of: /\sformat-version\s*=\s*"([^"]*)"/).map { String($0.1) }
        return [name, version].compactMap { $0 }.filter { !$0.isEmpty }.joined(separator: " ")
    }

    private static let twine2Marker = Data("<tw-storydata".utf8)
    private static let twine1Marker = Data("<div id=\"storeArea\"".utf8)
}
