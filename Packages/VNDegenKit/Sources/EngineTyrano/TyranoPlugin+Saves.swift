import Foundation
import VNCore
import WebRuntime

extension TyranoPlugin {
    /// Games with `configSave=file` keep `<projectID>_sf.sav`, `<projectID>_tyrano_data.sav`, and the like at the top of
    /// the game folder, beside the PC executable. Electron builds among them also keep the save key in localStorage, and
    /// it goes with the saves, so another VNDegen device can open them. Games that save to localStorage keep it in the
    /// app's data directory, where only other VNDegen devices can use it.
    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let game = TyranoGame(folder: folder) else { return [] }
        let storage = WebStorage(dataDirectory: dataDirectory)
        if game.savesToFiles {
            let files = SaveLocation(directory: folder) { Self.saveFileName($0, projectID: game.projectID) }
            guard game.packaging == .electron else { return [files] }
            let keyFile = WebStorage.fileName(for: Self.saveKeyName(projectID: game.projectID))
            return [files, SaveLocation(directory: storage.directory) { WebStorage.relativePath($0) == keyFile ? keyFile : nil }]
        }
        return [storage.saveLocation]
    }

    /// `C:/Games/Test Game 01/testgame01_sf.sav` maps to `testgame01_sf.sav`. A Mac build run from
    /// a quarantined folder saves the same names to `~/_TyranoGameData/`, which maps the same way.
    static func saveFileName(_ path: String, projectID: String) -> String? {
        guard let name = path.split(separator: "/").last.map(String.init) else { return nil }
        let lower = name.lowercased()
        guard lower.hasPrefix(projectID.lowercased() + "_"), lower.hasSuffix(".sav") else { return nil }
        return name
    }

    /// The localStorage key of an Electron build's save key. kag.js names it `$.getExePath() + "_" + projectID`, and
    /// getExePath cuts `app.getAppPath()` at `Game.app/Contents/Resources/app`, which `runtime(for:)` puts at the top of
    /// the game folder, so it is `/game/`.
    static func saveKeyName(projectID: String) -> String {
        "\(GameFolder.virtualRoot)/_\(projectID)"
    }

    /// Electron builds that save to files stamp `<projectID>_tyrano_data.sav` with a key kept in localStorage, and at
    /// start refuse saves whose stamp differs, after a prompt the host cannot answer yes to. Saves restored from the PC
    /// or another device carry that install's key, which this device does not have, so the key a restored save carries
    /// becomes this device's key before the game starts.
    static func adoptSaveKey(of game: TyranoGame, folder: URL, storage: WebStorage) throws {
        guard game.packaging == .electron, game.savesToFiles,
              let url = GameFolder(root: folder, webRoot: "").resolve(virtualPath: "\(GameFolder.virtualRoot)/\(game.projectID)_tyrano_data.sav"),
              let data = readBounded(url, limit: 64 << 20),
              let object = try? JSONSerialization.jsonObject(with: Data(jsUnescape(String(decoding: data, as: UTF8.self)).utf8)) as? [String: Any],
              let hash = object["hash"] as? String, !hash.isEmpty
        else { return }
        let name = saveKeyName(projectID: game.projectID)
        guard try storage.load()[name] != hash else { return }
        try storage.set(name, hash)
        tyranoLog.notice("took the save key of restored saves for \(game.projectID, privacy: .public)")
    }

    /// The file's bytes if it is a regular file of at most `limit` bytes. Reads no more than that, however large it is.
    static func readBounded(_ url: URL, limit: Int) -> Data? {
        guard (try? url.resourceValues(forKeys: [.isRegularFileKey]))?.isRegularFile == true,
              let handle = try? FileHandle(forReadingFrom: url) else { return nil }
        defer { try? handle.close() }
        guard let data = try? handle.read(upToCount: limit + 1), data.count <= limit else { return nil }
        return data
    }

    /// JavaScript's `unescape`: `%XX` is a Latin-1 character and `%uXXXX` a UTF-16 code unit.
    static func jsUnescape(_ text: String) -> String {
        let units = Array(text.utf16)
        func hex(_ start: Int, _ count: Int) -> UInt16? {
            guard start + count <= units.count else { return nil }
            var value: UInt16 = 0
            for unit in units[start ..< start + count] {
                guard unit < 128, let digit = Character(Unicode.Scalar(UInt8(unit))).hexDigitValue else { return nil }
                value = value << 4 | UInt16(digit)
            }
            return value
        }
        var out: [UInt16] = []
        var i = 0
        while i < units.count {
            if units[i] == UInt16(UInt8(ascii: "%")) {
                if i + 1 < units.count, units[i + 1] == UInt16(UInt8(ascii: "u")), let value = hex(i + 2, 4) {
                    out.append(value)
                    i += 6
                    continue
                }
                if let value = hex(i + 1, 2) {
                    out.append(value)
                    i += 3
                    continue
                }
            }
            out.append(units[i])
            i += 1
        }
        return String(decoding: out, as: UTF16.self)
    }
}
