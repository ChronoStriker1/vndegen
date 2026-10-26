import CArchive
import Foundation

nonisolated public enum ArchiveError: Error, Equatable, Sendable {
    case notAnArchive
    case severalArchives
    case missingVolume(Int)
    case passwordRequired
    case wrongPassword
    case unsupportedEncryption
    case unreadable(String)
    case notEnoughSpace(needed: Int64, available: Int64)
    /// A header gives a negative size, or sizes that add up past anything a device could hold.
    case impossibleSize
    /// Extraction produced more data than the headers listed, as a damaged archive or a decompression bomb does.
    case largerThanListed
    /// Free space fell to the reserve while extracting, because something else on the device used it meanwhile or the
    /// archive did not list its sizes.
    case ranOutOfSpace

    public var message: String {
        switch self {
        case .notAnArchive: "Choose a zip, RAR, or 7z archive."
        case .severalArchives: "Choose one archive at a time. For a multi-part archive, choose all of its parts."
        case let .missingVolume(part): "Part \(part) of this archive is missing. Choose all of its parts together."
        case .passwordRequired: "This archive needs a password."
        case .wrongPassword: "That password is not correct."
        case .unsupportedEncryption: "This archive uses RAR or 7z encryption, which VNDegen can't open. Extract it on a computer and copy the folder instead."
        case let .unreadable(detail): "The archive could not be read: \(detail)"
        case let .notEnoughSpace(needed, available):
            "The game needs \(needed.formatted(.byteCount(style: .file))), but only \(available.formatted(.byteCount(style: .file))) is free."
        case .impossibleSize: "The archive lists impossible file sizes. It is damaged or was made to fill the device."
        case .largerThanListed: "The archive holds more data than its file list says, so the import stopped. It is damaged or was made to fill the device."
        case .ranOutOfSpace: "The device is almost out of space, so the import stopped. Free up space and try again."
        }
    }
}

/// One forward pass over an archive with the system libarchive.
nonisolated final class ArchiveReader {
    struct Entry {
        enum Kind { case file, directory, other }
        /// The name exactly as stored, used to match the zip central directory.
        let rawName: [UInt8]
        /// Safe relative path components: no empty, `.`, or `..` parts.
        let path: [String]
        let kind: Kind
        let size: Int64?
        let isEncrypted: Bool
    }

    private let archive: OpaquePointer

    init(_ volumes: [URL], password: String?) throws(ArchiveError) {
        archive = archive_read_new()
        _ = archive_read_support_format_zip_seekable(archive)
        _ = archive_read_support_format_rar(archive)
        _ = archive_read_support_format_rar5(archive)
        _ = archive_read_support_format_7zip(archive)
        if let password { _ = archive_read_add_passphrase(archive, password) }

        let paths = volumes.map { strdup($0.path) }
        defer { paths.forEach { free($0) } }
        var pointers = paths.map { $0.map { UnsafePointer($0) } } + [nil]
        let result = archive_read_open_filenames(archive, &pointers, 1 << 20)
        if result != ARCHIVE_OK {
            let error = currentError()
            _ = archive_read_free(archive)
            throw error
        }
    }

    deinit { _ = archive_read_free(archive) }

    /// Whether this format's reader can decrypt. libarchive decrypts zip but not RAR or 7z.
    var canDecrypt: Bool { archive_read_format_capabilities(archive) & ARCHIVE_READ_FORMAT_CAPS_ENCRYPT_DATA != 0 }

    func next() throws(ArchiveError) -> Entry? {
        var entry: OpaquePointer?
        let result = archive_read_next_header(archive, &entry)
        if result == ARCHIVE_EOF { return nil }
        guard result == ARCHIVE_OK || result == ARCHIVE_WARN, let entry else { throw currentError() }

        let cName = archive_entry_pathname(entry) ?? archive_entry_pathname_utf8(entry)
        let rawName = cName.map { Array(UnsafeRawBufferPointer(start: $0, count: strlen($0))) } ?? []
        let kind: Entry.Kind = switch archive_entry_filetype(entry) {
        case AE_IFREG: .file
        case AE_IFDIR: .directory
        default: .other
        }
        return Entry(
            rawName: rawName,
            path: Self.pathComponents(Self.decodeName(rawName)),
            kind: kind,
            size: archive_entry_size_is_set(entry) != 0 ? archive_entry_size(entry) : nil,
            isEncrypted: archive_entry_is_encrypted(entry) != 0
        )
    }

    /// Hands each block of the current entry's data to `body` with its offset in the file.
    func readData(_ body: (UnsafeRawBufferPointer, Int64) throws -> Void) throws {
        var buffer: UnsafeRawPointer?
        var size = 0
        var offset: Int64 = 0
        while true {
            let result = archive_read_data_block(archive, &buffer, &size, &offset)
            if result == ARCHIVE_EOF { return }
            guard result == ARCHIVE_OK || result == ARCHIVE_WARN else { throw currentError() }
            if size > 0, let buffer { try body(UnsafeRawBufferPointer(start: buffer, count: size), offset) }
        }
    }

    private func currentError() -> ArchiveError {
        let message = archive_error_string(archive).map { String(cString: $0) } ?? "Unknown error"
        // libarchive reports these cases only through its message text.
        if message.contains("Incorrect passphrase") { return .wrongPassword }
        if message.contains("Passphrase required") { return .passwordRequired }
        if message.localizedCaseInsensitiveContains("encrypt") { return .unsupportedEncryption }
        if message.contains("Unrecognized archive format") { return .notAnArchive }
        return .unreadable(message)
    }

    /// Zip names written by Japanese Windows are Shift-JIS (code page 932) with no UTF-8 flag, and libarchive passes
    /// those bytes through unchanged. Names that are valid UTF-8 stay UTF-8.
    static func decodeName(_ bytes: [UInt8]) -> String {
        if let name = String(validating: bytes, as: UTF8.self) { return name }
        if let name = String(bytes: bytes, encoding: .windowsJapanese) { return name }
        return String(bytes: bytes, encoding: .windowsCP1252) ?? String(decoding: bytes, as: UTF8.self)
    }

    /// Splits on both separators, since Windows tools write `\`, and drops parts that could escape the folder.
    static func pathComponents(_ name: String) -> [String] {
        name.split(whereSeparator: { $0 == "/" || $0 == "\\" })
            .map(String.init)
            .filter { $0 != "." && $0 != ".." }
    }
}

extension String.Encoding {
    /// Code page 932, the Windows superset of Shift-JIS. Foundation's `.shiftJIS` lacks its NEC and IBM extensions.
    nonisolated static let windowsJapanese = String.Encoding(
        rawValue: CFStringConvertEncodingToNSStringEncoding(CFStringEncoding(CFStringEncodings.dosJapanese.rawValue)))
}
