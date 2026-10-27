import Foundation
import VNCore

extension NScripterPlugin {
    /// NScripter saves in the game's script folder, or in the folder the script's `savedir` command names there, on
    /// every platform. The runtime does the same, so a PC backup restores into the folder the game reads.
    nonisolated public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = NScripterLayout(FolderProbe(scanning: folder)) else { return [] }
        let root = layout.url(in: folder)
        guard let saveDir = Self.saveDirectory(root) else {
            return [SaveLocation(directory: root) { path in Self.engineFileName(path) }]
        }
        // The folder as it is on disk, matched without regard to case as the runtime matches it.
        let directory = FileCase.resolve(saveDir.joined(separator: "/"), in: root)
        return [SaveLocation(directory: directory) { path in Self.saveRelativePath(path, saveDir: saveDir) }]
    }

    /// The file name of a save the engine writes itself: `saveN.dat`, `gloval.sav`, and the read-text, file and label
    /// logs. In the game's root, other files are the game's own.
    nonisolated static func engineFileName(_ path: String) -> String? {
        guard let name = path.split(separator: "/").last.map(String.init) else { return nil }
        let lower = name.lowercased()
        if ["gloval.sav", "kidoku.dat", "nscrflog.dat", "nscrllog.dat"].contains(lower) { return name }
        guard lower.hasPrefix("save"), lower.hasSuffix(".dat") else { return nil }
        let number = lower.dropFirst(4).dropLast(4)
        return !number.isEmpty && number.allSatisfy { $0.isASCII && $0.isNumber } ? name : nil
    }

    /// `C:/Games/Game/save/save1.dat` maps to `save1.dat` when the script's `savedir` is `save`. Every file directly
    /// in that folder is a save, including screenshots the game names itself.
    nonisolated static func saveRelativePath(_ path: String, saveDir: [String]) -> String? {
        let parts = path.split(separator: "/").map(String.init)
        guard parts.count > saveDir.count + 1, let name = parts.last, ![".", ".."].contains(name) else { return nil }
        let folders = parts.dropLast().suffix(saveDir.count)
        return zip(folders, saveDir).allSatisfy({ $0.lowercased() == $1.lowercased() }) ? name : nil
    }

    /// The folders of the script's `savedir "name"` command, nil when the script has none or it leaves the game folder.
    nonisolated static func saveDirectory(_ root: URL) -> [String]? {
        guard let script = scriptBytes(root) else { return nil }
        // Commands ignore case. Other bytes become '?', one byte each, so offsets in the text are offsets in the script.
        let text = String(decoding: script.map { $0 < 0x80 ? $0 : 0x3F }, as: UTF8.self).lowercased()
        guard let match = text.firstMatch(of: /^[ \t]*savedir[ \t]+"([^"\r\n]*)"/.anchorsMatchLineEndings()) else { return nil }
        let start = text.utf8.distance(from: text.utf8.startIndex, to: match.1.startIndex)
        let name = String(decoding: script[start..<start + match.1.utf8.count], as: UTF8.self)
        let parts = name.split(whereSeparator: { $0 == "\\" || $0 == "/" }).map(String.init)
        guard !parts.isEmpty, !parts.contains(where: { $0 == "." || $0 == ".." || $0.contains(":") }) else { return nil }
        return parts
    }

    /// The script: the numbered text files in order, or the decoded nscript.dat when there are none.
    nonisolated private static func scriptBytes(_ root: URL) -> [UInt8]? {
        let names = (try? FileManager.default.contentsOfDirectory(atPath: root.path)) ?? []
        let numbered = names.compactMap { name -> (Int, String)? in
            guard let match = name.lowercased().wholeMatch(of: /(\d{1,2})\.txt/), let number = Int(match.1) else { return nil }
            return (number, name)
        }.sorted { $0 < $1 }
        let read = { (name: String) in FileManager.default.contents(atPath: root.appendingPathComponent(name).path) }
        if !numbered.isEmpty { return numbered.compactMap { read($0.1) }.reduce(into: []) { $0 += $1 } }
        guard let file = names.first(where: { $0.lowercased() == "nscript.dat" }), let data = read(file) else { return nil }
        return data.map { $0 ^ 0x84 }
    }
}
