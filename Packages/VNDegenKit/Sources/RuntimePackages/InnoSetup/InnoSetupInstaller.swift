// Reads Inno Setup 5 installers (setup data versions 5.0.0 through 5.6.x, ANSI and unicode builds) well enough to
// extract the files they install into {app}. It finds the setup loader's offset table in the PE resource (5.1.5 and
// later) or at the fixed pointer older loaders use, reads the LZMA1-compressed setup headers, and takes from them the
// license text, the directory and file entries, and the data entries. File data may be embedded in the setup program
// or split into setup-N.bin slice files, and stored, zlib, bzip2 or LZMA1 compressed; executables get the x86 call
// filter undone. Every file is checked against its stored MD5 or SHA-1. Encrypted installers, LZMA2 data, Inno Setup 6,
// earlier than 5, and the ISX variant are refused as unsupported. It was checked against the official RPG Maker XP,
// VX and VX Ace RTP installers (Inno Setup 5.2.3, 5.1.10 and 5.4.2 unicode).
//
// Ported from innoextract (https://github.com/dscharrer/innoextract): the {app} path handling follows its file name
// mapping. Altered from the original: only {app} files are kept, and conflicting entries resolve to the last one.
// innoextract's notice follows, as its license requires.
//
//   Copyright (C) 2011-2020 Daniel Scharrer <daniel@constexpr.org>
//
//   This software is provided 'as-is', without any express or implied
//   warranty.  In no event will the author(s) be held liable for any damages
//   arising from the use of this software.
//
//   Permission is granted to anyone to use this software for any purpose,
//   including commercial applications, and to alter it and redistribute it
//   freely, subject to the following restrictions:
//
//   1. The origin of this software must not be misrepresented; you must not
//      claim that you wrote the original software. If you use this software
//      in a product, an acknowledgment in the product documentation would be
//      appreciated but is not required.
//   2. Altered source versions must be plainly marked as such, and must not be
//      misrepresented as being the original software.
//   3. This notice may not be removed or altered from any source distribution.

import Foundation

/// An Inno Setup installer whose {app} files can be extracted.
struct InnoSetupInstaller {
    let headers: InnoHeaders
    private let setup: RandomAccessFile

    /// Reads the headers of the setup program at `url`. Slice files, if any, must sit beside it.
    init(setup url: URL) throws {
        setup = try RandomAccessFile(url: url)
        headers = try InnoHeaders(file: setup)
    }

    var licenseText: String? { headers.licenseText }

    /// The {app} files to extract, keyed by data entry, each with the relative paths it is written to.
    func plan() -> (directories: [String], files: [Int: [String]]) {
        var directories: [String] = []
        for directory in headers.directories where !directory.deleteAfterInstall {
            if let path = Self.appRelativePath(directory.name), !path.isEmpty { directories.append(path) }
        }
        // Paths compare without case, as on Windows. A later entry for the same path replaces an earlier one.
        var chosen: [String: (path: String, location: Int)] = [:]
        for file in headers.files where file.location < headers.dataEntries.count && !file.deleteAfterInstall {
            guard let path = Self.appRelativePath(file.destination), !path.isEmpty else { continue }
            let key = path.lowercased()
            // The uninstaller keeps the case of the path it replaces, as innoextract does.
            let kept = file.isUninstaller ? chosen[key]?.path ?? path : path
            chosen[key] = (kept, file.location)
        }
        var files: [Int: [String]] = [:]
        for (path, location) in chosen.values { files[location, default: []].append(path) }
        return (directories, files)
    }

    /// Writes the {app} files under `destination`, which must exist, reporting progress each time another percent of
    /// the files is done. Throws CancellationError when the task is cancelled, and leaves whatever it wrote for the
    /// caller to remove.
    func extractAppFiles(to destination: URL, progress: (_ done: Int, _ total: Int) -> Void) throws {
        let (directories, files) = plan()
        try extract(files, directories: directories, to: destination, progress: progress)
    }

    /// Writes each data entry in `files` to its relative paths under `destination`, checking it against its digest.
    func extract(_ files: [Int: [String]], directories: [String] = [], to destination: URL,
                 progress: (_ done: Int, _ total: Int) -> Void) throws {
        let manager = FileManager.default
        for directory in directories {
            try manager.createDirectory(at: destination.appending(path: directory), withIntermediateDirectories: true)
        }
        let total = files.values.reduce(0) { $0 + $1.count }
        var done = 0
        progress(0, total)

        // Read each chunk once, front to back, writing its files in order of their offset.
        let byChunk = Dictionary(grouping: files.keys) { headers.dataEntries[$0].chunkKey }
        let slices = InnoSliceReader(setup: setup, headers: headers)
        var buffer = [UInt8](repeating: 0, count: 1 << 18)
        for key in byChunk.keys.sorted() {
            let locations = byChunk[key]!.sorted { headers.dataEntries[$0].fileOffset < headers.dataEntries[$1].fileOffset }
            var stream: InnoChunkStream?
            var position: UInt64 = 0
            for location in locations {
                try Task.checkCancellation()
                let entry = headers.dataEntries[location]
                if stream == nil || entry.fileOffset < position {
                    stream = try InnoChunkStream(slices: slices, entry: entry)
                    position = 0
                }
                try stream!.skip(entry.fileOffset - position)

                let outputs = try files[location]!.map { path in
                    let url = destination.appending(path: path)
                    try manager.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
                    guard manager.createFile(atPath: url.path, contents: nil) else {
                        throw RuntimeInstallError.corrupt("cannot create \(path)")
                    }
                    return try FileHandle(forWritingTo: url)
                }
                defer { for output in outputs { try? output.close() } }
                var digest = FileDigest(entry.checksum)
                var decoder = InnoExecutableDecoder(entry.filter)
                func write(_ bytes: [UInt8]) throws {
                    digest.update(bytes)
                    let data = Data(bytes)
                    for output in outputs { try output.write(contentsOf: data) }
                }
                var left = entry.fileSize
                while left > 0 {
                    try Task.checkCancellation()
                    let step = Int(min(UInt64(buffer.count), left))
                    try buffer.withUnsafeMutableBytes { try stream!.read(into: UnsafeMutableRawBufferPointer(rebasing: $0[0..<step])) }
                    try write(decoder.process(Array(buffer[0..<step])))
                    left -= UInt64(step)
                }
                try write(decoder.finish())
                position = entry.fileOffset + entry.fileSize
                guard digest.matches(entry.checksum) else {
                    throw RuntimeInstallError.corrupt("checksum mismatch for \(files[location]![0])")
                }
                let before = done * 100 / total
                done += outputs.count
                if done * 100 / total > before { progress(done, total) }
            }
        }
    }

    /// The path below {app} that an installer destination such as `{app}\Graphics\Titles\001-Title01.jpg` names, with
    /// `/` separators, or nil for destinations outside {app}. Other constants become their names, and characters
    /// Windows forbids become `$`, as innoextract does.
    static func appRelativePath(_ destination: String) -> String? {
        guard destination.lowercased().hasPrefix("{app}") else { return nil }
        var expanded = ""
        var rest = destination.dropFirst(5)
        while let character = rest.popFirst() {
            if character == "{" {
                if rest.first == "{" {
                    rest.removeFirst()
                    expanded.append("{")
                } else {
                    let name = rest.prefix { $0 != "}" }
                    rest = rest.dropFirst(name.count + 1)
                    expanded += name.map(Self.safe)
                }
            } else {
                expanded.append(Self.safe(character))
            }
        }
        var segments: [Substring] = []
        for segment in expanded.split(whereSeparator: { $0 == "\\" || $0 == "/" }) where segment != "." {
            if segment == ".." { _ = segments.popLast() } else { segments.append(segment) }
        }
        return segments.joined(separator: "/")
    }

    private static func safe(_ character: Character) -> Character {
        if let ascii = character.asciiValue, ascii < 32 || "<>:\"|?*".utf8.contains(ascii) { return "$" }
        return character
    }
}

extension InnoHeaders.DataEntry {
    /// Orders chunks by where they start.
    var chunkKey: ChunkKey { ChunkKey(slice: firstSlice, offset: chunkOffset) }

    struct ChunkKey: Hashable, Comparable {
        let slice: Int
        let offset: UInt64

        static func < (lhs: ChunkKey, rhs: ChunkKey) -> Bool { (lhs.slice, lhs.offset) < (rhs.slice, rhs.offset) }
    }
}
