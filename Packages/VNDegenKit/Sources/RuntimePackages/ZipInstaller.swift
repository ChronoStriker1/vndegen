import CLzmaDec
import Foundation

/// Takes an installer out of a downloaded zip.
enum ZipInstaller {
    /// Extracts the setup program at `setupPath` and the `<name>-N.bin` data files beside it into `folder`, and returns
    /// the extracted setup program.
    static func extract(_ zip: URL, setupPath: String, to folder: URL) throws -> URL {
        let setupName = (setupPath as NSString).lastPathComponent
        let setupFolder = (setupPath as NSString).deletingLastPathComponent
        let slicePrefix = (setupName as NSString).deletingPathExtension.lowercased() + "-"
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)

        guard let archive = archive_read_new() else { throw RuntimeInstallError.corrupt("cannot open the zip") }
        defer { archive_read_free(archive) }
        archive_read_support_format_zip_seekable(archive)
        guard archive_read_open_filename(archive, zip.path, 1 << 16) == ARCHIVE_OK else {
            throw RuntimeInstallError.corrupt("not a zip: \(message(archive))")
        }
        var found: URL?
        var entry: OpaquePointer?
        while true {
            try Task.checkCancellation()
            let status = archive_read_next_header(archive, &entry)
            if status == ARCHIVE_EOF { break }
            guard status == ARCHIVE_OK || status == ARCHIVE_WARN, let entry,
                  let rawPath = archive_entry_pathname(entry) else {
                throw RuntimeInstallError.corrupt("damaged zip: \(message(archive))")
            }
            let path = String(cString: rawPath)
            let name = (path as NSString).lastPathComponent
            guard archive_entry_filetype(entry) == AE_IFREG, (path as NSString).deletingLastPathComponent == setupFolder
            else { continue }
            let isSetup = name == setupName
            let isSlice = name.lowercased().hasPrefix(slicePrefix) && name.lowercased().hasSuffix(".bin")
            guard isSetup || isSlice else { continue }
            let target = folder.appending(path: name)
            try write(archive, to: target)
            if isSetup { found = target }
        }
        guard let found else {
            throw RuntimeInstallError.unsupportedInstaller("the download has no \(setupPath)")
        }
        return found
    }

    private static func message(_ archive: OpaquePointer) -> String {
        archive_error_string(archive).map { String(cString: $0) } ?? "unknown error"
    }

    private static func write(_ archive: OpaquePointer, to target: URL) throws {
        guard FileManager.default.createFile(atPath: target.path, contents: nil) else {
            throw CocoaError(.fileWriteUnknown, userInfo: [NSFilePathErrorKey: target.path])
        }
        let output = try FileHandle(forWritingTo: target)
        defer { try? output.close() }
        var block: UnsafeRawPointer?
        var size = 0
        var offset: Int64 = 0
        while true {
            try Task.checkCancellation()
            let status = archive_read_data_block(archive, &block, &size, &offset)
            if status == ARCHIVE_EOF { return }
            guard status == ARCHIVE_OK || status == ARCHIVE_WARN else {
                throw RuntimeInstallError.corrupt("damaged zip: \(message(archive))")
            }
            // Zip entries arrive in order, so offset only matters for sparse formats.
            if let block, size > 0 { try output.write(contentsOf: Data(bytes: block, count: size)) }
        }
    }
}
