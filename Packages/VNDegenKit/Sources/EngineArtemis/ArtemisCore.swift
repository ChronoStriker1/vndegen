import CArtemis
import Darwin
import Foundation

/// art3m1s-core's C API table (`art3m1s_api.h`), found at run time in the engine library the app links, so the package
/// builds and tests without it.
nonisolated struct ArtemisCore: @unchecked Sendable {
    let api: Art3m1sApiV1

    /// Nil when this build of VNDegen has no engine, or one without VNDegen's additions to the table.
    static let shared: ArtemisCore? = {
        // RTLD_DEFAULT, a C macro Swift cannot import.
        let everywhere = UnsafeMutableRawPointer(bitPattern: -2)
        guard let symbol = dlsym(everywhere, "art3m1s_get_api_v1") else { return nil }
        typealias GetAPI = @convention(c) (UnsafeMutablePointer<Int>?) -> UnsafePointer<Art3m1sApiV1>?
        var size = 0
        guard let table = unsafeBitCast(symbol, to: GetAPI.self)(&size),
              size >= MemoryLayout<Art3m1sApiV1>.size, table.pointee.struct_size >= MemoryLayout<Art3m1sApiV1>.size
        else { return nil }
        return ArtemisCore(api: table.pointee)
    }()

    /// A game's files, as the core finds them: its archives with their patches, or its unpacked folder, and the save
    /// folder, in any case.
    final class Files {
        let core: ArtemisCore
        let resources: OpaquePointer

        /// Mounts the game. Archive names are in the game's charset, Shift_JIS for these games; an archive whose names
        /// read as UTF-8 mounts as UTF-8.
        init?(core: ArtemisCore, folder: URL, layout: ArtemisLayout) {
            guard let resources = core.api.resources_create!() else { return nil }
            self.core = core
            self.resources = resources
            let mounted = switch layout.data {
            case let .archive(path, _): core.api.resources_mount_pfs!(resources, folder.appending(path: path).path, "shift_jis")
            case .folder: core.api.resources_mount_directory!(resources, folder.appending(path: layout.root).path)
            }
            guard mounted == 1 else { return nil }
        }

        deinit { core.api.resources_destroy!(resources) }

        func setSaveDirectory(_ directory: URL) {
            _ = core.api.resources_set_save_dir!(resources, directory.path)
        }

        /// A whole file, or nil when the game has none of that name.
        func read(_ path: String) -> Data? {
            let size = core.api.resources_file_size!(resources, path)
            guard size >= 0 else { return nil }
            var data = Data(count: Int(size))
            let read = data.withUnsafeMutableBytes { buffer in
                core.api.resources_read!(resources, path, 0, buffer.baseAddress?.assumingMemoryBound(to: UInt8.self), Int(size))
            }
            return read == size ? data : nil
        }
    }
}

/// The settings of a game's `system.ini` VNDegen needs, from its `[WINDOWS]` section.
nonisolated struct ArtemisSettings: Equatable, Sendable {
    /// The stage size the game draws at.
    var width = 1280, height = 720
    /// `SAVEPATH`, with forward slashes, and whether `SAVEPATH_CSIDL` puts it in a Windows user folder rather than
    /// beside the game.
    var savePath = "save"
    var inUserFolder = false

    init(ini: Data) {
        // The engine reads system.ini in Shift_JIS, as it reads the folder name it saves to.
        let text = String(data: ini, encoding: Self.windowsJapanese) ?? String(decoding: ini, as: UTF8.self)
        var section = ""
        for raw in text.split(whereSeparator: \.isNewline) {
            let line = raw.trimmingCharacters(in: .whitespaces)
            if line.hasPrefix("[") { section = line.uppercased(); continue }
            guard section == "[WINDOWS]", !line.hasPrefix(";"), !line.hasPrefix("//") else { continue }
            let setting = line.split(separator: "=", maxSplits: 1).map { $0.trimmingCharacters(in: .whitespaces) }
            guard setting.count == 2 else { continue }
            switch setting[0].uppercased() {
            case "WIDTH": width = Int(setting[1]) ?? width
            case "HEIGHT": height = Int(setting[1]) ?? height
            case "SAVEPATH": savePath = Self.folder(setting[1]) ?? savePath
            case "SAVEPATH_CSIDL": inUserFolder = (Int(setting[1]) ?? 0) != 0
            default: break
            }
        }
    }

    /// A Windows folder path as relative forward-slash components, as the core cleans it. The yen sign is the
    /// backslash of Shift_JIS.
    static func folder(_ raw: String) -> String? {
        let parts = raw.replacingOccurrences(of: "\\", with: "/").replacingOccurrences(of: "¥", with: "/")
            .split(separator: "/").map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty && $0 != "." && $0 != ".." && !$0.hasSuffix(":") }
        return parts.isEmpty ? nil : parts.joined(separator: "/")
    }

    /// `savePath` as a Windows that isn't Japanese spells it: the game hands Windows its Shift_JIS bytes, which such a
    /// system reads as cp1252.
    var windowsSavePath: String? {
        guard let bytes = savePath.data(using: Self.windowsJapanese) else { return nil }
        let spelled = String(String.UnicodeScalarView(bytes.map(Self.cp1252)))
        return spelled == savePath ? nil : spelled
    }

    /// Shift_JIS as Windows reads it, code page 932, with the NEC and IBM characters plain Shift_JIS lacks.
    static let windowsJapanese = String.Encoding(rawValue: CFStringConvertEncodingToNSStringEncoding(
        CFStringEncoding(CFStringEncodings.dosJapanese.rawValue)))

    /// A cp1252 byte as Windows reads it. The five bytes cp1252 leaves undefined read as the same code points, as
    /// Windows' own conversion does.
    static func cp1252(_ byte: UInt8) -> UnicodeScalar {
        let high: [UInt32] = [
            0x20AC, 0x81, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x8D,
            0x017D, 0x8F, 0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A,
            0x0153, 0x9D, 0x017E, 0x0178,
        ]
        let value = (0x80...0x9F).contains(byte) ? high[Int(byte) - 0x80] : UInt32(byte)
        return UnicodeScalar(value) ?? "?"
    }
}
