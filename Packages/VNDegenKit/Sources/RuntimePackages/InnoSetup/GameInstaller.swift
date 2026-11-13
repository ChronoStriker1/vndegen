import Foundation

public enum GameInstallerError: Error, Equatable, Sendable {
    case notAnInstaller
    case unsupported(String)
    case encrypted
    case repack
    case nothingToUnpack
    case missingPart(String)
    case damaged(String)

    init(_ error: Error) {
        switch error {
        case let error as GameInstallerError: self = error
        case RuntimeInstallError.unsupportedInstaller(let detail):
            if detail.contains("encrypted") {
                self = .encrypted
            } else if detail == "not an Inno Setup installer" || detail.contains("setup loader") {
                self = .notAnInstaller
            } else {
                self = .unsupported(detail)
            }
        case RuntimeInstallError.corrupt(let detail): self = .damaged(detail)
        default: self = .damaged(error.localizedDescription)
        }
    }
}

public struct GameInstaller: Sendable, Equatable {
    public struct GOGInfo: Sendable, Equatable, Decodable {
        public let name: String
        public let gameId: String?
        public let rootGameId: String?
    }

    public let appName: String
    public let gog: GOGInfo?
    public let languages: [String]
    public let language: String?
    public let fileCount: Int
    public let directoryCount: Int
    public let byteCount: Int64
    let directories: [String]
    let outputs: [InnoSetupInstaller.Output]

    public var title: String { gog?.name ?? appName }
    public var gameID: String? { gog?.gameId }

    public var isAddOn: Bool {
        guard let gog, let root = gog.rootGameId, let id = gog.gameId else { return false }
        return root != id
    }

    public static func isInstaller(_ url: URL) -> Bool {
        (try? InnoSetupInstaller(setup: url)) != nil
    }

    public static func open(setup: URL, slices: [URL], preferredLanguages: [String] = Locale.preferredLanguages)
        throws(GameInstallerError) -> GameInstaller {
        do {
            let installer = try InnoSetupInstaller(setup: setup, slices: slices)
            guard !installer.hasRepackUnpacker else { throw GameInstallerError.repack }
            let gogLanguages = installer.gogLanguages
            let languages = gogLanguages.isEmpty ? installer.headers.languages : gogLanguages
            let language = choose(from: languages, preferred: preferredLanguages)
            let (directories, outputs) = try installer.gameOutputs(language: language)
            guard !outputs.isEmpty else { throw GameInstallerError.nothingToUnpack }
            if let missing = installer.missingSliceFile(for: outputs) { throw GameInstallerError.missingPart(missing) }
            let gog = try installer.headers.isGOG ? readGOGInfo(installer, outputs: outputs) : nil
            let bytes = outputs.reduce(UInt64(0)) { total, output in
                let (sum, overflow) = total.addingReportingOverflow(installer.outputSize(output))
                return overflow ? .max : sum
            }
            guard bytes < 1 << 50 else { throw GameInstallerError.damaged("impossible file sizes") }
            return GameInstaller(
                appName: installer.headers.appName, gog: gog, languages: languages, language: language,
                fileCount: outputs.count, directoryCount: directories.count, byteCount: Int64(bytes),
                directories: directories, outputs: outputs)
        } catch {
            throw GameInstallerError(error)
        }
    }

    public func extract(setup: URL, slices: [URL], into folder: URL,
                        written: (_ bytes: Int, _ finishedFile: Bool) throws -> Void) throws {
        let installer: InnoSetupInstaller
        do {
            installer = try InnoSetupInstaller(setup: setup, slices: slices)
        } catch {
            throw GameInstallerError(error)
        }
        do {
            try installer.write(outputs, directories: directories, to: folder, matchingCase: true, written: written)
        } catch let error as RuntimeInstallError {
            throw GameInstallerError(error)
        }
    }

    static func choose(from languages: [String], preferred: [String]) -> String? {
        guard languages.count > 1 else { return languages.first }
        let english = Locale(identifier: "en")
        for identifier in preferred {
            let wanted = Locale.Language(identifier: identifier)
            if let exact = languages.first(where: { $0.caseInsensitiveCompare(identifier) == .orderedSame }) { return exact }
            guard let code = wanted.languageCode?.identifier else { continue }
            let script = wanted.script?.identifier
            let byCode = languages.filter { Locale.Language(identifier: $0).languageCode?.identifier == code }
            if let match = byCode.first(where: { script == nil || Locale.Language(identifier: $0).script?.identifier == script })
                ?? byCode.first {
                return match
            }
            if let name = english.localizedString(forLanguageCode: code),
               let match = languages.first(where: { $0.caseInsensitiveCompare(name) == .orderedSame }) {
                return match
            }
        }
        return languages.first { $0.caseInsensitiveCompare("en-US") == .orderedSame } ?? languages.first
    }

    private static func readGOGInfo(_ installer: InnoSetupInstaller, outputs: [InnoSetupInstaller.Output]) throws -> GOGInfo? {
        guard let output = outputs.first(where: { $0.path.wholeMatch(of: /goggame-\d+\.info/.ignoresCase()) != nil }),
              installer.outputSize(output) <= 1 << 20 else { return nil }
        let folder = FileManager.default.temporaryDirectory.appending(path: "vndegen-gog-\(UUID().uuidString)", directoryHint: .isDirectory)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        try installer.write([output], directories: [], to: folder, matchingCase: false) { _, _ in }
        let data = try Data(contentsOf: folder.appending(path: output.path))
        return try? JSONDecoder().decode(GOGInfo.self, from: data)
    }
}
