import Foundation

nonisolated struct SiglusLanguage: Equatable, Sendable {
    let code: String
    let pack: String

    var saveFolder: String { pack.isEmpty ? "savedata" : "savedata_\(pack.lowercased())" }

    static func choose(root: URL, preferred: [String] = Locale.preferredLanguages) -> SiglusLanguage {
        let names = (try? FileManager.default.contentsOfDirectory(atPath: root.path)) ?? []
        return choose(files: names, preferred: preferred)
    }

    static func choose(files: [String], preferred: [String]) -> SiglusLanguage {
        let shipped = Set(files.compactMap(packSuffix))
        guard !shipped.isEmpty else { return SiglusLanguage(code: "JP", pack: "") }
        let base = shipped.contains("EN") ? "JP" : "EN"
        for code in preferred.compactMap(code) {
            if code == base { return SiglusLanguage(code: code, pack: "") }
            if shipped.contains(code) { return SiglusLanguage(code: code, pack: code) }
        }
        return base == "EN" ? SiglusLanguage(code: "EN", pack: "") : SiglusLanguage(code: "EN", pack: "EN")
    }

    static func packSuffix(_ file: String) -> String? {
        let upper = file.uppercased()
        guard upper.hasPrefix("SCENE"), upper.hasSuffix(".PCK") else { return nil }
        let suffix = upper.dropFirst(5).dropLast(4)
        return suffix.isEmpty || !suffix.allSatisfy({ $0.isLetter && $0.isASCII }) ? nil : String(suffix)
    }

    static func code(_ tag: String) -> String? {
        let language = Locale.Language(identifier: tag)
        switch language.languageCode?.identifier {
        case "en": return "EN"
        case "de": return "DE"
        case "fr": return "FR"
        case "es": return "ES"
        case "ja": return "JP"
        case "zh":
            let traditional = language.script?.identifier == "Hant"
                || ["TW", "HK", "MO"].contains(language.region?.identifier ?? "")
            return traditional ? "ZHTW" : "ZH"
        default: return nil
        }
    }
}
