import Foundation
import VNCore

nonisolated struct PJADVLayout: Equatable, Sendable {
    var root: String
    var evidence: String

    init?(_ probe: FolderProbe) {
        var best: PJADVLayout?
        for entry in probe.entries.sorted() {
            let parts = entry.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
            guard parts.count <= 2, parts.last?.lowercased() == "archive.dat" else { continue }
            let folder = parts.dropLast().joined(separator: "/")
            if let current = best, parts.count - 1 >= current.depth { continue }
            guard let head = probe.read(entry, limit: 1 << 16), Self.isScenarioArchive(head) else { continue }
            best = PJADVLayout(root: folder, evidence: entry)
        }
        guard let best else { return nil }
        self = best
    }

    private init(root: String, evidence: String) {
        self.root = root
        self.evidence = evidence
    }

    private var depth: Int { root.isEmpty ? 0 : root.split(separator: "/").count }

    func url(in folder: URL) -> URL {
        root.isEmpty ? folder : folder.appendingPathComponent(root, isDirectory: true)
    }

    static func isScenarioArchive(_ head: Data) -> Bool {
        let bytes = [UInt8](head)
        guard bytes.count >= 16, bytes.starts(with: Array("GAMEDAT PAC".utf8)) else { return false }
        let nameLength = bytes[11] == UInt8(ascii: "K") ? 16 : bytes[11] == UInt8(ascii: "2") ? 32 : 0
        let count = Int(bytes[12]) | Int(bytes[13]) << 8 | Int(bytes[14]) << 16 | Int(bytes[15]) << 24
        guard nameLength > 0, count > 0, count < 4096 else { return false }
        var names = Set<String>()
        for i in 0..<count {
            let start = 16 + i * nameLength
            guard start + nameLength <= bytes.count else { break }
            let raw = bytes[start..<start + nameLength].prefix { $0 != 0 }
            names.insert(String(decoding: raw, as: UTF8.self).lowercased())
        }
        return names.contains("scenario.dat") && names.contains("textdata.bin")
    }
}
