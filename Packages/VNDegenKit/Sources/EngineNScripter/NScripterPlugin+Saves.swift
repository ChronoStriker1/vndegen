import Foundation
import VNCore

extension NScripterPlugin {
    /// NScripter saves in the game's script folder, or in the folder the script's `savedir` command names there, on
    /// every platform. The runtime does the same, so a PC backup restores into the folder the game reads.
    nonisolated public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = NScripterLayout(FolderProbe(scanning: folder)) else { return [] }
        let root = layout.url(in: folder)
        // A Ponscripter game's saves are the PC engine's files, but the runtime keeps them in vndegen-saves, never in
        // the game's own saves folder, which Steam may sync. A PC backup's saves/ files restore there.
        if layout.ponscripter {
            return [SaveLocation(directory: root.appendingPathComponent("vndegen-saves")) { path in
                Self.ponscripterSavePath(path)
            }]
        }
        guard let saveDir = Self.saveDirectory(root) else {
            return [SaveLocation(directory: root) { path in Self.engineFileName(path) }]
        }
        // The folder as it is on disk, matched without regard to case as the runtime matches it.
        let directory = FileCase.resolve(saveDir.joined(separator: "/"), in: root)
        return [SaveLocation(directory: directory) { path in Self.saveRelativePath(path, saveDir: saveDir) }]
    }

    /// `…/saves/save1.dat` maps to `save1.dat`, and the thumbnails the Umineko games write, `…/saves/screenshot/ss1.bmp`,
    /// to `screenshot/ss1.bmp`; the same files in `vndegen-saves` map the same way. The PC engine's files are
    /// `saveN.dat`, `global.sav` and `envdata`. VNDegen's own preference file there isn't a save.
    nonisolated static func ponscripterSavePath(_ path: String) -> String? {
        let parts = path.split(separator: "/").map(String.init)
        guard let name = parts.last else { return nil }
        let lower = name.lowercased()
        let folder = { (back: Int) in parts.count > back ? parts[parts.count - 1 - back].lowercased() : "" }
        let isSaveFolder = { (name: String) in name == "saves" || name == "vndegen-saves" }
        if folder(1) == "screenshot", isSaveFolder(folder(2)), lower.hasSuffix(".bmp") { return "screenshot/" + name }
        guard isSaveFolder(folder(1)) else { return nil }
        if ["global.sav", "envdata"].contains(lower) { return name }
        guard lower.hasPrefix("save"), lower.hasSuffix(".dat") else { return nil }
        let number = lower.dropFirst(4).dropLast(4)
        return !number.isEmpty && number.allSatisfy { $0.isASCII && $0.isNumber } ? name : nil
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

    /// The folders of the script's `savedir "name"` command below the script folder, nil when there are none.
    nonisolated static func saveDirectory(_ root: URL) -> [String]? {
        guard let script = scriptBytes(root) else { return nil }
        // Commands ignore case. Other bytes become '?', one byte each, so offsets in the text are offsets in the script.
        let text = String(decoding: script.map { $0 < 0x80 ? $0 : 0x3F }, as: UTF8.self).lowercased()
        guard let match = text.firstMatch(of: /^[ \t]*savedir[ \t]+"([^"\r\n]*)"/.anchorsMatchLineEndings()) else { return nil }
        let start = text.utf8.distance(from: text.utf8.startIndex, to: match.1.startIndex)
        // Scripts are Shift-JIS, as the runtime reads them.
        let bytes = script[start..<start + match.1.utf8.count]
        let name = String(bytes: bytes, encoding: .shiftJIS) ?? String(decoding: bytes, as: UTF8.self)
        // ".." climbs but never above the game folder, as the runtime resolves it.
        var parts: [String] = []
        for part in name.split(whereSeparator: { $0 == "\\" || $0 == "/" }).map(String.init) where part != "." {
            if part == ".." { _ = parts.popLast() } else { parts.append(part) }
        }
        return parts.isEmpty ? nil : parts
    }

    /// The script, chosen as the runtime chooses it: `0.txt` .. `99.txt` when there is a `0.txt`, else `00.txt` ..
    /// `99.txt` when there is a `00.txt`, else the decoded `nscript.dat`.
    nonisolated private static func scriptBytes(_ root: URL) -> [UInt8]? {
        let names = (try? FileManager.default.contentsOfDirectory(atPath: root.path)) ?? []
        let byLower = Dictionary(names.map { ($0.lowercased(), $0) }, uniquingKeysWith: min)
        let read = { (name: String) in FileManager.default.contents(atPath: root.appendingPathComponent(name).path) }
        // Each file ends its last line, and carriage returns go, as the runtime joins them.
        for twoDigit in [false, true] where byLower[twoDigit ? "00.txt" : "0.txt"] != nil {
            let files = (0..<100).compactMap { byLower[twoDigit ? String(format: "%02d.txt", $0) : "\($0).txt"] }
            var script: [UInt8] = []
            for data in files.compactMap(read) {
                script += data
                if let last = script.last, last != 0x0A { script.append(0x0A) }
            }
            return script.filter { $0 != 0x0D }
        }
        guard let file = byLower["nscript.dat"], let data = read(file) else { return nil }
        return data.map { $0 ^ 0x84 }.filter { $0 != 0x0D }
    }
}
