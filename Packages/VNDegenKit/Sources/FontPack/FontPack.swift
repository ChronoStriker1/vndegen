import CVNFont
import Foundation
import Synchronization
import VNCore

/// Windows font substitutes, as Proton provides them: freely licensed fonts with the metrics of the Windows fonts games
/// ask for by name, such as MS PGothic or Arial. The app bundles the Latin and Japanese ones, and the player can
/// download the Chinese and Korean ones as runtime packages. One table, `fonts.tsv` in `Engines/Shared/fonts`, maps
/// the Windows names to them, and every engine reads it: native engines through `vnfont.h`, the rest through this API.
nonisolated public enum FontPack {
    /// A substitute font file. When the caller asked for bold or italic and the table has no such face, `embolden` or
    /// `slant` says to draw the regular face bolder or slanted.
    public struct Match: Sendable, Equatable {
        public let file: URL
        /// The face's index in a font collection, 0 for a single font.
        public let face: Int
        public let bold: Bool
        public let italic: Bool
        public let embolden: Bool
        public let slant: Bool
    }

    /// One line of the table whose font is installed.
    public struct Entry: Sendable, Equatable {
        /// The Windows name as the table spells it, such as "MS PGothic" or "ＭＳ Ｐゴシック".
        public let name: String
        public let match: Match
    }

    /// The fonts the app bundles, with the table, in the app's resources.
    public static var bundledFolder: URL? { Bundle.main.url(forResource: "fonts", withExtension: nil) }

    private static let state = Mutex<[URL]>([])

    /// The folders the table's fonts come from, the bundled folder first, as last configured. Native engines pass them
    /// to `vnfont_init` with `table`.
    public static var folders: [URL] { state.withLock { $0 } }

    /// The table, in the first folder.
    public static var table: URL? { folders.first?.appending(path: "fonts.tsv") }

    /// Reads the table in `folders[0]` and finds its fonts in `folders`, in order. Call it at launch and after a font
    /// package is installed or removed. Returns false when the table can't be read, which leaves no substitutes.
    @discardableResult
    public static func configure(folders: [URL]) -> Bool {
        state.withLock { current in
            current = folders
            guard let first = folders.first else { return vnfont_init("", nil, 0) == 0 }
            let paths = folders.map { strdup($0.path) }
            defer { paths.forEach { free($0) } }
            let table = first.appending(path: "fonts.tsv").path
            return paths.map { UnsafePointer($0) }.withUnsafeBufferPointer {
                vnfont_init(table, $0.baseAddress, Int32($0.count))
            } == 0
        }
    }

    /// Configures the bundled fonts and every font package `installedFolder` reports installed.
    @discardableResult
    public static func configure(installedFolder: (RuntimePackage) -> URL?) -> Bool {
        configure(folders: [bundledFolder].compactMap(\.self) + packages.compactMap(installedFolder))
    }

    /// The substitute for the Windows font `name`. Case, spaces, full-width letters, and a leading @ don't matter. Nil
    /// when the table has none, or its font isn't installed.
    public static func resolve(_ name: String, bold: Bool = false, italic: Bool = false) -> Match? {
        var match = vnfont_match()
        guard vnfont_resolve(name, bold ? 1 : 0, italic ? 1 : 0, &match) == 1 else { return nil }
        return Match(match)
    }

    /// Every line of the table whose font is installed, in table order.
    public static var entries: [Entry] {
        var entry = vnfont_entry()
        return (0..<vnfont_count()).compactMap { index in
            guard vnfont_entry_at(index, &entry) == 1 else { return nil }
            return Entry(name: string(entry.name), match: Match(entry.match))
        }
    }

    /// The table's names that appear in `text`, such as a game's scripts, matched the way `resolve` matches names, so
    /// "ＭＳ Ｐゴシック" in the text finds "MS PGothic". Only names whose fonts are installed count.
    public static func referencedNames(in text: String) -> Set<String> {
        let haystack = normalized(text)
        return Set(entries.map(\.name).filter { name in normalized(name).map { !$0.isEmpty && haystack?.contains($0) == true } ?? false })
    }

    private static func normalized(_ text: String) -> String? {
        let size = text.utf8.count + 1
        return withUnsafeTemporaryAllocation(of: CChar.self, capacity: size) { buffer in
            vnfont_normalize(text, buffer.baseAddress, size) == 1 ? String(cString: buffer.baseAddress!) : nil
        }
    }

    /// An installed font file by its file name, for serving to a web page. Nil for any name the table doesn't use.
    public static func file(named name: String) -> URL? {
        entries.first { $0.match.file.lastPathComponent == name }?.match.file
    }

    fileprivate static func string<T>(_ tuple: T) -> String {
        withUnsafeBytes(of: tuple) { String(decoding: $0.prefix { $0 != 0 }, as: UTF8.self) }
    }
}

nonisolated extension FontPack.Match {
    fileprivate init(_ match: vnfont_match) {
        file = URL(fileURLWithPath: FontPack.string(match.path))
        face = Int(match.face)
        bold = match.bold != 0
        italic = match.italic != 0
        embolden = match.embolden != 0
        slant = match.slant != 0
    }
}

nonisolated extension FontPack {
    /// The Chinese and Korean fonts, which are too large to bundle: Source Han Sans from Adobe's release, Regular and
    /// Bold. Proton builds its Microsoft YaHei, SimSun, and Malgun Gothic from the same fonts.
    public static let packages = [chinese, korean]

    private static let release = "https://github.com/adobe-fonts/source-han-sans/releases/download/2.005R/"
    private static let licensePage = URL(string: "https://github.com/adobe-fonts/source-han-sans/blob/release/LICENSE.txt")!

    public static let chinese = RuntimePackage(
        id: "fonts-source-han-sans-cn", name: "Chinese fonts (Source Han Sans CN)",
        download: URL(string: release + "19_SourceHanSansCN.zip")!,
        sha256: "3a769d1b082ebd813cdb4c06ea57d29b340bce548edcc8e61976b78ca28e6236", size: 50_700_226,
        installer: .zip(files: ["SubsetOTF/CN/SourceHanSansCN-Regular.otf", "SubsetOTF/CN/SourceHanSansCN-Bold.otf", "LICENSE.txt"]),
        licensePage: licensePage)

    public static let korean = RuntimePackage(
        id: "fonts-source-han-sans-kr", name: "Korean fonts (Source Han Sans KR)",
        download: URL(string: release + "18_SourceHanSansKR.zip")!,
        sha256: "ada0398122ebfebcae9efc1c6bb1dc5512c03b8f54035a8f527cd82a3c0441ac", size: 26_022_476,
        installer: .zip(files: ["SubsetOTF/KR/SourceHanSansKR-Regular.otf", "SubsetOTF/KR/SourceHanSansKR-Bold.otf", "LICENSE.txt"]),
        licensePage: licensePage)
}
