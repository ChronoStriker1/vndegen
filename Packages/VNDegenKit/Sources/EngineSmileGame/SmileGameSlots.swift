import Foundation
import VNCore

nonisolated enum SmileGameSlots {
    static let count = 40

    struct Slot: Equatable {
        var index: Int
        var written: Date?

        var title: String {
            guard let written else { return "Slot \(index + 1): empty" }
            return "Slot \(index + 1): \(written.formatted(date: .abbreviated, time: .shortened))"
        }
    }

    enum Choice: Equatable {
        case save(Int)
        case load(Int)
    }

    static func fileName(_ index: Int) -> String { String(format: "Save_%02d.sgs", index) }

    static func list(in directory: URL) -> [Slot] {
        (0..<count).map { index in
            let url = FileCase.resolve(fileName(index), in: directory)
            let written = (try? FileManager.default.attributesOfItem(atPath: url.path))?[.modificationDate] as? Date
            return Slot(index: index, written: written)
        }
    }

    static func saveID(_ index: Int) -> String { "save.\(index)" }
    static func loadID(_ index: Int) -> String { "load.\(index)" }

    static func choice(_ id: String) -> Choice? {
        let parts = id.split(separator: ".")
        guard parts.count == 2, let index = Int(parts[1]), (0..<count).contains(index) else { return nil }
        switch parts[0] {
        case "save": return .save(index)
        case "load": return .load(index)
        default: return nil
        }
    }
}
