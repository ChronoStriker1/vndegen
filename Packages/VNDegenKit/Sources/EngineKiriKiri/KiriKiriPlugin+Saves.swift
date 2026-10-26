import Foundation
import VNCore

extension KiriKiriPlugin {
    /// KiriKiri saves to `savedata/` in the game's root, the folder holding its `.exe`, on every platform, unless the
    /// game's `<exe>.cf` moves it. Some games save to `System.appDataPath` or `System.personalPath` instead, the
    /// AppData and Documents folders on Windows, which VNDegen keeps in the game's data directory.
    nonisolated public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = KiriKiriLayout(scanning: folder) else { return [] }
        let paths = KiriKiriPaths(folder: folder, dataDirectory: dataDirectory, layout: layout)
        let appData = paths.appData, documents = paths.documents
        var locations = [
            Self.userFolderLocation(appData, rule: Self.appDataRelativePath),
            Self.userFolderLocation(documents, rule: Self.documentsRelativePath),
        ]
        if let saves = Self.saveFolder(paths) {
            // A game that keeps a save folder of that name under AppData, or under VNDegen's own AppData or Documents,
            // saves there, not in its root. A game installed under Windows Documents still saves in its root.
            let location = SaveLocation(directory: paths.gameRoot.appendingPathComponent(saves.joined(separator: "/"), isDirectory: true)) { path in
                Self.appDataRelativePath(path) == nil && Self.ownUserFolderPath(path, directory: appData) == nil
                    && Self.ownUserFolderPath(path, directory: documents) == nil ? Self.saveRelativePath(path, folder: saves) : nil
            }
            locations.insert(location, at: 0)
        }
        return locations
    }

    /// The save folder in the game's root as path components: `savedata`, or the folder the `-datapath` option of the
    /// game's `<exe>.cf` names, resolved the way change 30 in `Engines/KiriKiri/UPSTREAM.md` does. Nil when that folder
    /// is in the AppData or Documents stand-in, whose own locations cover it, or is the root itself, which holds the
    /// game's files too.
    nonisolated static func saveFolder(_ paths: KiriKiriPaths) -> [String]? {
        guard let option = configuredDataPath(gameRoot: paths.gameRoot), !option.isEmpty else { return ["savedata"] }
        let root = paths.gameRoot.path + "/"
        var path = option
        for (macro, folder) in [("$(exepath)", root), ("$(personalpath)", paths.documents.path), ("$(appdatapath)", paths.appData.path),
                                ("$(vistapath)", paths.appData.path), ("$(savedgamespath)", paths.documents.path)] {
            path = path.replacingOccurrences(of: macro, with: folder.hasSuffix("/") ? String(folder.dropLast()) : folder)
        }
        path = path.replacingOccurrences(of: "\\", with: "/")
        if !path.hasPrefix("/") { path = root + path }
        if !path.hasSuffix("/") { path += "/" }
        // The engine ignores a path that could leave the game's folders, and keeps `savedata`.
        guard !path.contains("/../"), !path.contains(":"), let folders = storageFolders(path) else { return ["savedata"] }
        let inside = { (folder: URL) in
            let top = storageFolders(folder.path) ?? []
            return folders.starts(with: top) ? Array(folders.dropFirst(top.count)) : nil
        }
        if let rest = inside(paths.gameRoot) { return rest.isEmpty ? nil : rest.map(String.init) }
        return inside(paths.appData) != nil || inside(paths.documents) != nil ? nil : ["savedata"]
    }

    /// The folders of `path` as KiriKiri resolves a storage name: a folder named only with dots climbs one folder fewer
    /// than it has dots, so `.` stays and `...` climbs two, and empty folders drop. Nil when it climbs above the top.
    nonisolated static func storageFolders(_ path: String) -> [Substring]? {
        var folders: [Substring] = []
        for part in path.split(separator: "/") {
            guard part.allSatisfy({ $0 == "." }) else {
                folders.append(part)
                continue
            }
            guard folders.count >= part.count - 1 else { return nil }
            folders.removeLast(part.count - 1)
        }
        return folders
    }

    /// The value of the first `datapath` option in the `.cf` beside the game's `.exe`, read as the engine reads it: the
    /// `.exe` that has a `.cf` wins, then a lone `.exe`. Each line is `NAME=VALUE`, a quoted value is a TJS string, and
    /// a line starting with `;` is a comment.
    nonisolated static func configuredDataPath(gameRoot: URL) -> String? {
        let files = (try? FileManager.default.contentsOfDirectory(at: gameRoot, includingPropertiesForKeys: [.isRegularFileKey])) ?? []
        let names = files.filter { (try? $0.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile) == true }.map(\.lastPathComponent)
        let stem = { (name: String, ext: String) in
            name.lowercased().hasSuffix(ext) && name.count > ext.count ? String(name.lowercased().dropLast(ext.count)) : nil
        }
        let exes = names.filter { stem($0, ".exe") != nil }.sorted()
        let configs = Set(names.compactMap { stem($0, ".cf") })
        guard let exe = exes.first(where: { stem($0, ".exe").map(configs.contains) == true }) ?? (exes.count == 1 ? exes[0] : nil),
              let contents = try? Data(contentsOf: gameRoot.appendingPathComponent(String(exe.dropLast(4)) + ".cf"))
        else { return nil }
        // A CRLF line end is one Character, so splitting on "\n" alone would leave a Windows file one line.
        for line in String(decoding: contents, as: UTF8.self).split(omittingEmptySubsequences: false, whereSeparator: \.isNewline) {
            guard !line.hasPrefix(";"), let equals = line.firstIndex(of: "="), line[..<equals] == "datapath" else { continue }
            let value = line[line.index(after: equals)...]
            return value.first == "\"" || value.first == "'" ? tjsString(value) ?? String(value) : String(value)
        }
        return nil
    }

    /// A TJS string literal, as `TJSParseString` in `tjs2/tjsLex.cpp` reads one: `\x` takes up to four hex digits,
    /// `\0` octal digits, and literals separated only by spaces join. Nil when the literal is not closed.
    nonisolated static func tjsString(_ literal: Substring) -> String? {
        var rest = literal.unicodeScalars[...]
        guard let quote = rest.popFirst() else { return nil }
        var result = String.UnicodeScalarView()
        while let scalar = rest.popFirst() {
            switch scalar {
            case "\\":
                guard let escaped = rest.popFirst() else { return nil }
                switch escaped {
                case "x", "X", "0":
                    let (radix, limit) = escaped == "0" ? (8, Int.max) : (16, 4)
                    var code: UInt32 = 0, count = 0
                    while count < limit, let next = rest.first, next.isASCII, let digit = Character(next).hexDigitValue, digit < radix {
                        code = code &* UInt32(radix) &+ UInt32(digit)
                        count += 1
                        rest.removeFirst()
                    }
                    guard !rest.isEmpty else { return nil }
                    result.append(Unicode.Scalar(code) ?? "\u{FFFD}")
                case "a": result.append("\u{07}")
                case "b": result.append("\u{08}")
                case "f": result.append("\u{0C}")
                case "n": result.append("\n")
                case "r": result.append("\r")
                case "t": result.append("\t")
                case "v": result.append("\u{0B}")
                default: result.append(escaped)
                }
            case quote:
                let next = rest.drop { $0.properties.isWhitespace }
                guard next.first == quote else { return String(result) }
                rest = next.dropFirst()
            default:
                result.append(scalar)
            }
        }
        return nil
    }

    nonisolated static func appDataDirectory(_ dataDirectory: URL) -> URL {
        dataDirectory.appendingPathComponent("AppData", isDirectory: true)
    }

    nonisolated static func documentsDirectory(_ dataDirectory: URL) -> URL {
        dataDirectory.appendingPathComponent("Documents", isDirectory: true)
    }

    /// With `folder` `["savedata"]`, `/home/deck/Games/vn/<game>/savedata/datasc.ksd` and
    /// `C:/Games/<game>/savedata/datasc.ksd` map to `datasc.ksd`, and subfolders of `savedata/` are kept. The last
    /// place `folder` appears wins, compared without case. Nil for a path that names no file or could leave the save
    /// folder, such as `savedata/../startup.tjs`, because restore writes each file at the path this returns.
    nonisolated static func saveRelativePath(_ path: String, folder: [String]) -> String? {
        let parts = path.split(separator: "/", omittingEmptySubsequences: false)
        let lower = parts.map { $0.lowercased() }, wanted = folder.map { $0.lowercased() }
        guard lower.count > wanted.count,
              let start = (0...(lower.count - wanted.count)).last(where: { Array(lower[$0..<($0 + wanted.count)]) == wanted })
        else { return nil }
        return insideFolder(parts[(start + wanted.count)...].joined(separator: "/"))
    }

    /// `C:/Users/<user>/AppData/Roaming/<rest>` on Windows and `.../drive_c/users/steamuser/AppData/Roaming/<rest>` in
    /// Proton map to `<rest>`.
    nonisolated static func appDataRelativePath(_ path: String) -> String? {
        userFolderRelativePath(path, folders: [["appdata", "roaming"]])
    }

    /// `C:/Users/<user>/Documents/<rest>` on Windows and `.../drive_c/users/steamuser/My Documents/<rest>` in Proton map
    /// to `<rest>`. So does `Saved Games`, which the engine keeps in the Documents stand-in.
    nonisolated static func documentsRelativePath(_ path: String) -> String? {
        userFolderRelativePath(path, folders: [["documents"], ["my documents"], ["saved games"]])
    }

    /// A location in the game's data directory that stands in for a Windows user folder. Besides `rule`, it recognizes
    /// the paths `ownUserFolderPath` does.
    nonisolated private static func userFolderLocation(_ directory: URL, rule: @escaping @Sendable (String) -> String?) -> SaveLocation {
        SaveLocation(directory: directory) { path in
            ownUserFolderPath(path, directory: directory).map(insideFolder) ?? rule(path)
        }
    }

    /// The rest of `path` when it is in `directory`, a user folder in the game's data directory, or in a copy of it:
    /// `/VNDegen/<game>/<folder>/<rest>`, the path a first backup records, or a data directory's
    /// `GameData/<game>/<folder>/<rest>` from any VNDegen install. Not yet checked with `insideFolder`.
    nonisolated private static func ownUserFolderPath(_ path: String, directory: URL) -> String? {
        let local = directory.standardizedFileURL.path + "/"
        if path.hasPrefix(local) { return String(path.dropFirst(local.count)) }
        let parts = path.split(separator: "/", omittingEmptySubsequences: false)
        let root = parts.dropFirst().first == "VNDegen" ? 1 : parts.firstIndex(of: "GameData")
        guard let root, parts.count > root + 3, parts[root + 2] == directory.lastPathComponent else { return nil }
        return parts[(root + 3)...].joined(separator: "/")
    }

    /// The rest of `path` after `users/<user>/` and one of `folders`, compared without case.
    nonisolated private static func userFolderRelativePath(_ path: String, folders: [[String]]) -> String? {
        let parts = path.split(separator: "/", omittingEmptySubsequences: false)
        let lower = parts.map { $0.lowercased() }
        guard let users = lower.firstIndex(of: "users") else { return nil }
        let start = users + 2
        for folder in folders where lower.count > start + folder.count && Array(lower[start..<(start + folder.count)]) == folder {
            return insideFolder(parts[(start + folder.count)...].joined(separator: "/"))
        }
        return nil
    }

    /// Nil for a path that names no file or could leave its folder, such as `../library.json`. A backup's paths come
    /// from another machine, and restore writes each file at the path this returns.
    nonisolated private static func insideFolder(_ relative: String) -> String? {
        let parts = relative.split(separator: "/", omittingEmptySubsequences: false)
        return parts.contains { ["", ".", ".."].contains($0) } ? nil : relative
    }
}
