import Foundation
import VNCore

extension RenPyRuntime {
    /// The Ren'Py release VNDegen bundles for this line.
    nonisolated public var version: String { self == .renPy8 ? "8.5.3" : "7.8.7" }
}

/// What happened the last time a Ren'Py game ran on this device, kept in `<data directory>/renpy-boot.json`. iOS runs
/// Ren'Py 7 games on Ren'Py 8, and this is how the game page knows whether one worked.
nonisolated public struct RenPyBootRecord: Codable, Equatable, Sendable {
    public enum Result: String, Codable, Sendable {
        /// The game was still starting when VNDegen last closed, such as after a crash.
        case started
        case reachedMainMenu
        /// Ren'Py stopped with an error before the main menu showed.
        case failed
        /// The player left before the main menu showed.
        case stopped
    }

    public var result: Result
    /// Such as "8.5.3".
    public var runtime: String
    /// The first line of Ren'Py's traceback that names the error.
    public var error: String?

    public init(result: Result, runtime: String, error: String? = nil) {
        self.result = result
        self.runtime = runtime
        self.error = error
    }

    static func file(_ dataDirectory: URL) -> URL { dataDirectory.appendingPathComponent("renpy-boot.json") }

    /// `zz_vndegen.rpy` creates this once the main menu shows.
    static func mainMenuMarker(_ dataDirectory: URL) -> URL { dataDirectory.appendingPathComponent("logs/main-menu") }

    static func traceback(_ dataDirectory: URL) -> URL { dataDirectory.appendingPathComponent("logs/traceback.txt") }

    public static func read(_ dataDirectory: URL) -> RenPyBootRecord? {
        guard let data = try? Data(contentsOf: file(dataDirectory)) else { return nil }
        return try? JSONDecoder().decode(RenPyBootRecord.self, from: data)
    }

    func write(_ dataDirectory: URL) {
        try? FileManager.default.createDirectory(at: dataDirectory, withIntermediateDirectories: true)
        try? JSONEncoder().encode(self).write(to: Self.file(dataDirectory), options: .atomic)
    }

    /// Clears what the last run left in `logs/` and records that a run started.
    static func started(dataDirectory: URL, runtime: RenPyRuntime) {
        try? FileManager.default.removeItem(at: mainMenuMarker(dataDirectory))
        try? FileManager.default.removeItem(at: traceback(dataDirectory))
        RenPyBootRecord(result: .started, runtime: runtime.version).write(dataDirectory)
    }

    /// Records how the run that just ended went. A game that reached its main menu counts as working, even if it
    /// stopped with an error later.
    static func finished(dataDirectory: URL, runtime: RenPyRuntime) {
        let error = firstError([traceback(dataDirectory)])
        let result: Result = FileManager.default.fileExists(atPath: mainMenuMarker(dataDirectory).path) ? .reachedMainMenu
            : error == nil ? .stopped : .failed
        RenPyBootRecord(result: result, runtime: runtime.version, error: error).write(dataDirectory)
    }

    /// The first line that names a Python exception in the first of `files` that has one.
    public static func firstError(_ files: [URL]) -> String? {
        for file in files {
            guard let text = try? String(contentsOf: file, encoding: .utf8) else { continue }
            if let line = text.split(separator: "\n").first(where: { $0.firstMatch(of: #/^[A-Za-z.]*(Error|Exception)\b/#) != nil }) {
                return String(line.prefix(120))
            }
        }
        return nil
    }
}

/// How Ren'Py 7 games run on Ren'Py 8, the only runtime on iPhone and iPad.
nonisolated public enum RenPyCompatibility {
    /// Ren'Py 7 and 6.99 games from the maintainer's library, booted on Ren'Py 8 with
    /// `scripts/renpy-compat.sh <folder> --runtime 8` on 2026-10-26, by `RenPyLayout.fingerprint`. A nil cause means the
    /// game reached its main menu, or for the three whose title menu is a choice menu, its story. Games are listed by
    /// fingerprint so the list names no game.
    static let tested: [String: String?] = [
        "daa863b2a5f11e3d": nil, // 6.99.12
        "abbb157a6989f798": nil, // 6.99.14
        "9a820c0701dee901": nil, // 7.1.0
        "4a51c6f94d82b794": nil, // 7.3.5
        "24a1c66bb18aaf6c": nil, // 7.3.5
        "8b6e21829176c2b5": nil, // 7.3.5
        "7d1649bef15c14e9": nil, // 7.3.5, with Python 2 division
        "e12b4b91abd1d52e": nil, // 7.3.5
        "ba2a7a4b0b92c1ae": nil, // 7.3.5
        "a2d1fd94843448be": "its achievements need a Steam module from the modified copy of Ren'Py it ships", // 7.3.5
        "51027959cee46da4": nil, // 7.4.2
        "23c79f4861d83268": nil, // 7.4.9
        "6fafeb91c9bf2fba": nil, // 7.4.11
        "bfe55bc05a5d09a6": nil, // 7.5.1
        "dd46b3f7ed2394b7": nil, // 7.5.1
        "9673208eeba44067": nil, // 7.5.1
    ]

    /// The note for the game page. On iOS, a Ren'Py 7 game gets how it fared: on this device if it ran here, else in
    /// the library test.
    static func note(layout: RenPyLayout, fingerprint: String?, record: RenPyBootRecord?) -> String? {
        let version = layout.version.map { "Ren'Py \($0)" } ?? "Ren'Py 7"
        let outcome: String? = switch record?.result {
        case .reachedMainMenu?: "It reached its main menu on this device."
        case .failed?: "Last time it stopped with an error: \(record?.error ?? "unknown error")."
        case .started?: "VNDegen closed while it was starting last time."
        case .stopped?, nil: nil
        }
        guard layout.runtime == .renPy7 else { return record?.result == .failed ? outcome : nil }
        let known: String = switch fingerprint.flatMap({ tested[$0] }) {
        case .some(nil): "It is known to work."
        case let .some(cause?): "It is known not to work yet: \(cause)."
        case nil: "It hasn't been tested on Ren'Py 8 yet."
        }
        return "A \(version) game, which runs on Ren'Py \(RenPyRuntime.renPy8.version) here. \(outcome ?? known)"
    }

    /// The major and minor version, such as (7, 4) for "7.4.2".
    static func release(_ version: String) -> [Int]? {
        let parts = version.split(separator: ".").prefix(2).compactMap { Int($0) }
        return parts.count == 2 ? parts : nil
    }

    /// Ren'Py loads saves from older releases but not from newer ones. Tested on a Ren'Py 7.4.2 game: saves written by
    /// Ren'Py 7.8.7 and 8.5.3 both fail to load in its own engine.
    static func backupWarning(gameVersion: String?, runtime: RenPyRuntime) -> String? {
        guard let gameVersion, let own = release(gameVersion), let running = release(runtime.version),
              own.lexicographicallyPrecedes(running) else { return nil }
        return "Saves made here use Ren'Py \(runtime.version), and the PC version of this game uses Ren'Py \(gameVersion). "
            + "Older Ren'Py can't load saves from a newer release, so the PC version won't load the saves in this backup. "
            + "VNDegen on other devices will."
    }
}
