import Foundation
import VNCore

nonisolated struct SystemAoiLayout: Equatable, Sendable {
    var root: String
    var evidence: String

    init?(_ probe: FolderProbe) {
        var best: SystemAoiLayout?
        for entry in probe.entries.sorted() {
            let parts = entry.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
            guard parts.count <= 2, parts.last?.lowercased() == "box.vfs" else { continue }
            let folder = parts.dropLast().joined(separator: "/")
            if let current = best, parts.count - 1 >= current.depth { continue }
            guard let head = probe.read(entry, limit: 1 << 16), Self.isScriptArchive(head) else { continue }
            best = SystemAoiLayout(root: folder, evidence: entry)
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

    static func isScriptArchive(_ head: Data) -> Bool {
        let bytes = [UInt8](head)
        func u16(_ at: Int) -> Int { Int(bytes[at]) | Int(bytes[at + 1]) << 8 }
        func u32(_ at: Int) -> Int { u16(at) | u16(at + 2) << 16 }
        guard bytes.count >= 16, bytes[0] == UInt8(ascii: "V"), bytes[1] == UInt8(ascii: "F"), u16(2) >= 0x200 else {
            return false
        }
        let count = u16(4), entrySize = u16(6)
        guard count > 0, entrySize >= 0x17 else { return false }
        let table = 16 + count * entrySize
        guard table + 4 <= bytes.count else { return false }
        let chars = u32(table)
        guard table + 4 + chars * 2 <= bytes.count else { return false }
        var units: [UInt16] = []
        for i in 0..<chars { units.append(UInt16(u16(table + 4 + i * 2))) }
        let names = Set(String(decoding: units, as: UTF16.self).split(separator: "\0").map { $0.lowercased() })
        return names.contains("init.box") && names.contains("pro.box")
    }
}
