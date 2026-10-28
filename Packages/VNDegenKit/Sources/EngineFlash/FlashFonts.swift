import FontPack
import Foundation
import WebRuntime

/// The fonts Flash's generic device fonts stand for, as Flash Player on Japanese Windows draws them: Latin text in
/// Arial, Times New Roman, or Courier New, and the rest in MS UI Gothic, MS PMincho, or MS Gothic. Each becomes one
/// family in the page, whose faces come from VNDegen's Windows font substitutes, since Ruffle's canvas text asks the
/// browser for a single family.
nonisolated enum FlashFonts {
    struct Face: Encodable, Equatable {
        /// The Windows name, which the device's own font of that name answers to first, as Arial does on Apple systems.
        var local: String
        /// The substitute's file, served at `GameFolder.fontPath`.
        var file: String
        var bold: Bool
        var latin: Bool
    }

    struct Family: Encodable, Equatable {
        var name: String
        var faces: [Face]
    }

    struct Config: Encodable {
        var base: String
        var families: [String: Family]
    }

    /// Ruffle's default font slot, the family's name in the page, and the Windows fonts for Latin and other text.
    static let generic = [
        ("sans", "VNDegen Flash sans", "Arial", "MS UI Gothic"),
        ("serif", "VNDegen Flash serif", "Times New Roman", "MS PMincho"),
        ("typewriter", "VNDegen Flash typewriter", "Courier New", "MS Gothic"),
    ]

    static func families(resolve: (String, Bool) -> FontPack.Match? = { FontPack.resolve($0, bold: $1) }) -> [String: Family] {
        var families: [String: Family] = [:]
        for (slot, name, latin, other) in generic {
            let faces = [(latin, true), (other, false)].flatMap { windowsName, isLatin in
                [false, true].compactMap { bold in
                    // A bold that is the regular face drawn bolder is left to the browser, which does the same.
                    resolve(windowsName, bold).flatMap { match in
                        bold && match.embolden ? nil : Face(local: windowsName, file: match.file.lastPathComponent, bold: bold, latin: isLatin)
                    }
                }
            }
            families[slot] = Family(name: name, faces: faces)
        }
        return families
    }

    /// `flash-page.js` with the families filled in.
    static func pageScript(_ source: String) throws -> String {
        let config = Config(base: GameFolder.origin + GameFolder.fontPath, families: families())
        let json = String(decoding: try JSONEncoder().encode(config), as: UTF8.self)
        return source.replacingOccurrences(of: "__VNDEGEN_FLASH_FONTS__", with: json)
    }
}
