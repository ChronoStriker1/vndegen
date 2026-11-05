import Foundation
import VNCore

/// The company and product a Unity game names in `<Game>_Data/app.info`, one per line. Unity keeps the game's
/// `persistentDataPath` under them, `AppData/LocalLow/<company>/<product>` on Windows and
/// `~/Library/Application Support/<company>/<product>` on a Mac, and its PlayerPrefs under the registry key
/// `HKEY_CURRENT_USER\Software\<company>\<product>` on Windows.
nonisolated struct UnityAppInfo: Equatable, Sendable {
    var company: String
    var product: String

    /// Nil unless both names are there and safe to use as one folder and one registry key name each.
    init?(company: String, product: String) {
        guard Self.isUsable(company), Self.isUsable(product) else { return nil }
        self.company = company
        self.product = product
    }

    init?(text: String) {
        let lines = text.split(omittingEmptySubsequences: false, whereSeparator: \.isNewline).map(String.init)
        guard lines.count >= 2 else { return nil }
        self.init(company: lines[0], product: lines[1])
    }

    /// Reads `app.info` in the game's data folder, whatever case the folder and file have on disk.
    init?(folder: URL, layout: UnityLayout) {
        let data = FileCase.resolve(layout.dataFolder + "/app.info", in: folder)
        guard let bytes = try? Data(contentsOf: data), bytes.count < 4096, let text = String(data: bytes, encoding: .utf8) else { return nil }
        self.init(text: text)
    }

    /// No path separators, registry key separators or brackets, which a `.reg` file's key line can't hold, and no
    /// control characters, so neither name can lead outside its folder or into another key.
    static func isUsable(_ name: String) -> Bool {
        !name.isEmpty && name.count <= 255 && name != "." && name != ".."
            && !name.unicodeScalars.contains { "\\/[]".unicodeScalars.contains($0) || $0.properties.generalCategory == .control }
    }

    /// The only registry key VNDegen ever moves for this game.
    var playerPrefsKey: String { #"HKEY_CURRENT_USER\Software\"# + company + "\\" + product }

    /// The rest of a backup's path after `AppData/LocalLow/<company>/<product>/` from Windows, Proton or a CrossOver
    /// bottle, or after `Library/Application Support/<company>/<product>/` from a Mac build, compared without case. Nil
    /// for any other path, and for one that could leave the folder.
    func relativePath(_ path: String) -> String? {
        let parts = path.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
        let lower = parts.map { $0.lowercased() }
        let company = company.lowercased(), product = product.lowercased()
        for prefix in [["appdata", "locallow"], ["library", "application support"]] {
            let wanted = prefix + [company, product]
            guard lower.count > wanted.count,
                  let start = (0...(lower.count - wanted.count)).last(where: { Array(lower[$0..<($0 + wanted.count)]) == wanted })
            else { continue }
            let rest = parts[(start + wanted.count)...]
            return rest.contains { ["", ".", ".."].contains($0) } ? nil : rest.joined(separator: "/")
        }
        return nil
    }
}
