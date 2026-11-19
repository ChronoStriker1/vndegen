import CBRUNS
import Foundation
import VNCore

extension EngineID {
    nonisolated public static let bruns = EngineID("bruns")
}

nonisolated public struct BrunsPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .bruns, displayName: "Bruns", shortName: "Bruns", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = BrunsLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = BrunsLayout(FolderProbe(scanning: launch.folder)) else { throw BrunsError.notBruns }
        let root = layout.url(in: launch.folder)
        guard let product = Self.product(root) else { throw BrunsError.noConfig }
        return BrunsSession(launch, root: root, saveDirectory: Self.saveDirectory(launch.dataDirectory, product: product))
    }

    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = BrunsLayout(FolderProbe(scanning: folder)),
              let product = Self.product(layout.url(in: folder)) else { return [] }
        let directory = Self.saveDirectory(dataDirectory, product: product)
        return [SaveLocation(directory: directory) { path in Self.saveName(path, product: product) }]
    }

    public var defaultControls: ControlLayout { .visualNovel }

    nonisolated struct Product: Equatable, Sendable {
        var author: String
        var name: String
    }

    nonisolated static func product(_ root: URL) -> Product? {
        var author = [CChar](repeating: 0, count: 256)
        var name = [CChar](repeating: 0, count: 256)
        guard vnbruns_product(root.path, &author, author.count, &name, name.count) else { return nil }
        return Product(author: String(decoding: author.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self),
                       name: String(decoding: name.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self))
    }

    nonisolated static func saveDirectory(_ dataDirectory: URL, product: Product) -> URL {
        dataDirectory.appending(path: "AppData/Roaming", directoryHint: .isDirectory)
            .appending(path: product.author, directoryHint: .isDirectory)
            .appending(path: product.name, directoryHint: .isDirectory)
            .appending(path: "save", directoryHint: .isDirectory)
    }

    nonisolated static func saveName(_ path: String, product: Product) -> String? {
        let parts = path.replacingOccurrences(of: "\\", with: "/").split(separator: "/").map(String.init)
        guard let name = parts.last, name.lowercased() == "bruns-system.dat" else { return nil }
        let folders = parts.dropLast().map { $0.precomposedStringWithCanonicalMapping.lowercased() }
        guard folders.count >= 3, folders[folders.count - 1] == "save",
              folders[folders.count - 2] == product.name.precomposedStringWithCanonicalMapping.lowercased(),
              folders[folders.count - 3] == product.author.precomposedStringWithCanonicalMapping.lowercased() else { return nil }
        return name
    }
}

nonisolated public enum BrunsError: Error, LocalizedError {
    case notBruns
    case noConfig

    public var errorDescription: String? {
        switch self {
        case .notBruns: "No Bruns game was found in this folder."
        case .noConfig: "This game's ams.cfg couldn't be read."
        }
    }
}
