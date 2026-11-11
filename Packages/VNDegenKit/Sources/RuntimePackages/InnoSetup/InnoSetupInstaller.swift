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

import CryptoKit
import Foundation
import VNCore

/// An Inno Setup installer whose {app} files can be extracted.
struct InnoSetupInstaller {
    let headers: InnoHeaders
    private let setup: RandomAccessFile
    private let sliceFiles: [URL]

    /// Reads the headers of the setup program at `url`. Slice files, if any, must sit beside it.
    init(setup url: URL, slices: [URL] = []) throws {
        setup = try RandomAccessFile(url: url)
        headers = try InnoHeaders(file: setup)
        sliceFiles = slices
    }

    var licenseText: String? { headers.licenseText }

    struct Output: Sendable, Equatable {
        struct Part: Sendable, Equatable {
            let location: Int
            let inflatedSize: UInt64?
        }

        var path: String
        var parts: [Part]
        var md5: [UInt8]?
    }

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
        let outputs = files.sorted { $0.key < $1.key }.flatMap { location, paths in
            paths.map { Output(path: $0, parts: [.init(location: location, inflatedSize: nil)]) }
        }
        let total = outputs.count
        var done = 0
        progress(0, total)
        try write(outputs, directories: directories, to: destination, matchingCase: false) { _, finished in
            guard finished else { return }
            let before = done * 100 / total
            done += 1
            if done * 100 / total > before { progress(done, total) }
        }
    }

    func outputSize(_ output: Output) -> UInt64 {
        output.parts.reduce(0) { $0 + ($1.inflatedSize ?? headers.dataEntries[$1.location].fileSize) }
    }

    var gogLanguages: [String] {
        var names: Set<String> = []
        for file in headers.files {
            guard let check = Self.call(file.check, "check_if_install"), let languages = check.first else { continue }
            for constraint in Self.constraints(languages) { names.insert(constraint.name) }
        }
        return names.sorted()
    }

    var hasRepackUnpacker: Bool {
        headers.files.contains { file in
            let name = file.destination.split(whereSeparator: { $0 == "\\" || $0 == "/" }).last?.lowercased()
            return file.destination.lowercased().hasPrefix("{tmp}") && ["isdone.dll", "unarc.dll"].contains(name)
        }
    }

    func gameOutputs(language: String?) throws -> (directories: [String], outputs: [Output]) {
        let gogLanguages = Set(gogLanguages)
        var directories: [String] = []
        for directory in headers.directories where !directory.deleteAfterInstall {
            if let path = Self.appRelativePath(directory.name), !path.isEmpty, !Self.isRedistributable(path) {
                directories.append(path)
            }
        }

        var chosen: [String: Output] = [:]
        var order: [String] = []
        func add(_ output: Output) {
            let key = output.path.lowercased()
            if chosen.updateValue(output, forKey: key) == nil { order.append(key) }
        }
        func wanted(_ file: InnoHeaders.File) -> Bool {
            if let check = Self.call(file.check, "check_if_install") {
                if let names = check.first, !Self.passes(Self.constraints(names), language: language, all: gogLanguages) {
                    return false
                }
                if check.count >= 2, !Self.passesArchitecture(Self.constraints(check[1])) { return false }
            } else if let language, headers.languages.contains(language), !file.languages.isEmpty {
                let names = file.languages.split(whereSeparator: { " ,;".contains($0) }).map(String.init)
                if !names.contains(where: { $0.caseInsensitiveCompare(language) == .orderedSame }) { return false }
            }
            return true
        }

        var galaxy: (output: Output, wanted: Bool, remaining: Int)?
        for file in headers.files {
            if let start = Self.call(file.beforeInstall, "before_install") ?? Self.call(file.beforeInstall, "before_install_dependency") {
                guard galaxy == nil else { throw RuntimeInstallError.corrupt("incomplete GOG Galaxy file") }
                let name = start.count >= 2 && !start[1].isEmpty ? start[1] : file.destination
                let path = name.hasPrefix("{") ? Self.appRelativePath(name) : Self.appRelativePath("{app}\\" + name)
                let parts = start.count >= 3 ? max(1, Int(start[2]) ?? 1) : 1
                galaxy = (Output(path: path ?? "", parts: [], md5: Self.md5(start[0])), wanted(file) && path?.isEmpty == false, parts)
            }
            if let info = Self.call(file.afterInstall, "after_install") ?? Self.call(file.afterInstall, "after_install_dependency") {
                guard var current = galaxy, info.count >= 3, let size = UInt64(info[2]),
                      file.location < headers.dataEntries.count else {
                    throw RuntimeInstallError.corrupt("damaged GOG Galaxy file part \(file.destination)")
                }
                current.output.parts.append(.init(location: file.location, inflatedSize: size))
                current.remaining -= 1
                if current.remaining == 0 {
                    if current.wanted, !Self.isRedistributable(current.output.path) { add(current.output) }
                    galaxy = nil
                } else {
                    galaxy = current
                }
                continue
            }
            guard galaxy == nil else { throw RuntimeInstallError.corrupt("incomplete GOG Galaxy file") }
            guard file.location < headers.dataEntries.count, !file.deleteAfterInstall, !file.isUninstaller,
                  let path = Self.appRelativePath(file.destination), !path.isEmpty, !Self.isRedistributable(path),
                  wanted(file) else { continue }
            add(Output(path: path, parts: [.init(location: file.location, inflatedSize: nil)]))
        }
        guard galaxy == nil else { throw RuntimeInstallError.corrupt("incomplete GOG Galaxy file") }
        return (directories, order.compactMap { chosen[$0] })
    }

    func missingSliceFile(for outputs: [Output]) -> String? {
        let slices = InnoSliceReader(setup: setup, headers: headers, files: sliceFiles)
        guard !slices.isEmbedded else { return nil }
        var needed: Set<Int> = []
        for part in outputs.flatMap(\.parts) {
            let entry = headers.dataEntries[part.location]
            needed.formUnion(entry.firstSlice...max(entry.firstSlice, entry.lastSlice))
        }
        return needed.sorted().lazy.filter { slices.locate(slice: $0) == nil }.map { slices.expectedName(slice: $0) }.first
    }

    func write(_ outputs: [Output], directories: [String], to destination: URL, matchingCase: Bool,
               written: (_ bytes: Int, _ finishedFile: Bool) throws -> Void) throws {
        let manager = FileManager.default
        func url(_ path: String) -> URL {
            matchingCase ? FileCase.resolve(path, in: destination) : destination.appending(path: path)
        }
        for directory in directories {
            try manager.createDirectory(at: url(directory), withIntermediateDirectories: true)
        }

        struct Target {
            let output: Int
            let part: Int
            let offset: UInt64
            let inflatedSize: UInt64?
        }
        var targets: [Int: [Target]] = [:]
        for (index, output) in outputs.enumerated() {
            var offset: UInt64 = 0
            for (part, piece) in output.parts.enumerated() {
                guard piece.location < headers.dataEntries.count else {
                    throw RuntimeInstallError.corrupt("file \(output.path) points outside the installer")
                }
                targets[piece.location, default: []].append(Target(output: index, part: part, offset: offset, inflatedSize: piece.inflatedSize))
                offset += piece.inflatedSize ?? headers.dataEntries[piece.location].fileSize
            }
        }

        final class OpenOutput {
            let handle: FileHandle
            let url: URL
            var partsLeft: Int
            var nextPart = 0
            var hash: Insecure.MD5? = Insecure.MD5()

            init(handle: FileHandle, url: URL, parts: Int) {
                self.handle = handle
                self.url = url
                partsLeft = parts
            }
        }
        var open: [Int: OpenOutput] = [:]
        defer { for output in open.values { try? output.handle.close() } }

        func opened(_ index: Int) throws -> OpenOutput {
            if let output = open[index] { return output }
            let target = url(outputs[index].path)
            try manager.createDirectory(at: target.deletingLastPathComponent(), withIntermediateDirectories: true)
            guard manager.createFile(atPath: target.path, contents: nil) else {
                throw RuntimeInstallError.corrupt("cannot create \(outputs[index].path)")
            }
            let output = OpenOutput(handle: try FileHandle(forWritingTo: target), url: target, parts: outputs[index].parts.count)
            open[index] = output
            return output
        }

        func finish(_ index: Int) throws {
            guard let output = open.removeValue(forKey: index) else { return }
            try output.handle.close()
            if let expected = outputs[index].md5 {
                let digest: [UInt8]
                if let hash = output.hash {
                    digest = Array(hash.finalize())
                } else {
                    var hash = Insecure.MD5()
                    let reader = try FileHandle(forReadingFrom: output.url)
                    defer { try? reader.close() }
                    while let data = try reader.read(upToCount: 1 << 20), !data.isEmpty { hash.update(data: data) }
                    digest = Array(hash.finalize())
                }
                guard digest == expected else { throw RuntimeInstallError.corrupt("checksum mismatch for \(outputs[index].path)") }
            }
            try written(0, true)
        }

        for (index, output) in outputs.enumerated() where output.parts.isEmpty {
            _ = try opened(index)
            try finish(index)
        }

        // Read each chunk once, front to back, writing its files in order of their offset.
        let byChunk = Dictionary(grouping: targets.keys) { headers.dataEntries[$0].chunkKey }
        let slices = InnoSliceReader(setup: setup, headers: headers, files: sliceFiles)
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

                let sinks = try targets[location]!.map { target in
                    let output = try opened(target.output)
                    let inOrder = output.hash != nil && output.nextPart == target.part
                    if !inOrder { output.hash = nil }
                    try output.handle.seek(toOffset: target.offset)
                    return (target: target, output: output, inflater: try target.inflatedSize.map { _ in try ZlibDecoder() },
                            produced: UInt64(0), ended: false)
                }
                var outputsOfEntry = sinks
                var digest = FileDigest(entry.checksum)
                var decoder = InnoExecutableDecoder(entry.filter)
                var inflated = [UInt8](repeating: 0, count: 1 << 18)
                func emit(_ bytes: [UInt8]) throws {
                    guard !bytes.isEmpty else { return }
                    digest.update(bytes)
                    for index in outputsOfEntry.indices {
                        let output = outputsOfEntry[index].output
                        guard let inflater = outputsOfEntry[index].inflater else {
                            let data = Data(bytes)
                            try output.handle.write(contentsOf: data)
                            output.hash?.update(data: data)
                            try written(bytes.count, false)
                            continue
                        }
                        var consumed = 0
                        while consumed < bytes.count {
                            guard !outputsOfEntry[index].ended else {
                                throw RuntimeInstallError.corrupt("GOG Galaxy part of \(outputs[outputsOfEntry[index].target.output].path) is longer than its stream")
                            }
                            let (used, produced, ended) = try bytes.withUnsafeBytes { input in
                                try inflated.withUnsafeMutableBytes {
                                    try inflater.decode(UnsafeRawBufferPointer(rebasing: input[consumed...]), into: $0)
                                }
                            }
                            consumed += used
                            outputsOfEntry[index].ended = ended
                            outputsOfEntry[index].produced += UInt64(produced)
                            guard outputsOfEntry[index].produced <= outputsOfEntry[index].target.inflatedSize! else {
                                throw RuntimeInstallError.corrupt("GOG Galaxy part of \(outputs[outputsOfEntry[index].target.output].path) is too long")
                            }
                            if produced > 0 {
                                let data = Data(inflated[0..<produced])
                                try output.handle.write(contentsOf: data)
                                output.hash?.update(data: data)
                                try written(produced, false)
                            }
                            if used == 0, produced == 0 { break }
                        }
                    }
                }
                var left = entry.fileSize
                while left > 0 {
                    try Task.checkCancellation()
                    let step = Int(min(UInt64(buffer.count), left))
                    try buffer.withUnsafeMutableBytes { try stream!.read(into: UnsafeMutableRawBufferPointer(rebasing: $0[0..<step])) }
                    try emit(decoder.process(Array(buffer[0..<step])))
                    left -= UInt64(step)
                }
                try emit(decoder.finish())
                position = entry.fileOffset + entry.fileSize
                guard digest.matches(entry.checksum) else {
                    throw RuntimeInstallError.corrupt("checksum mismatch for \(outputs[targets[location]![0].output].path)")
                }
                for sink in outputsOfEntry {
                    if let size = sink.target.inflatedSize {
                        if !sink.ended, sink.produced < size {
                            var flushed = 0
                            repeat {
                                let (_, produced, ended) = try [UInt8]().withUnsafeBytes { input in
                                    try inflated.withUnsafeMutableBytes { try sink.inflater!.decode(input, into: $0) }
                                }
                                flushed = produced
                                if produced > 0 {
                                    let data = Data(inflated[0..<produced])
                                    try sink.output.handle.write(contentsOf: data)
                                    sink.output.hash?.update(data: data)
                                    try written(produced, false)
                                }
                                if ended { break }
                            } while flushed > 0
                        }
                    }
                }
                for sink in outputsOfEntry {
                    if let size = sink.target.inflatedSize, try sink.output.handle.offset() != sink.target.offset + size {
                        throw RuntimeInstallError.corrupt("GOG Galaxy part of \(outputs[sink.target.output].path) has the wrong size")
                    }
                    sink.output.nextPart = sink.target.part + 1
                    sink.output.partsLeft -= 1
                    if sink.output.partsLeft == 0 { try finish(sink.target.output) }
                }
            }
        }
        guard open.isEmpty else { throw RuntimeInstallError.corrupt("installer data ends before every file is complete") }
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

    private static func isRedistributable(_ path: String) -> Bool {
        path.split(separator: "/").first?.lowercased() == "__redist"
    }

    static func call(_ code: String, _ name: String) -> [String]? {
        let whitespace: Set<Character> = [" ", "\t", "\r", "\n"]
        var rest = code.drop { whitespace.contains($0) }
        guard rest.hasPrefix(name) else { return nil }
        rest = rest.dropFirst(name.count).drop { whitespace.contains($0) }
        guard rest.first == "(" else { return nil }
        rest = rest.dropFirst()
        var arguments: [String] = []
        while true {
            rest = rest.drop { whitespace.contains($0) }
            var argument = ""
            if rest.first == "'" {
                rest = rest.dropFirst()
                while true {
                    guard let quote = rest.firstIndex(of: "'") else { return nil }
                    argument += rest[..<quote]
                    rest = rest[rest.index(after: quote)...]
                    if rest.first == "'" {
                        argument.append("'")
                        rest = rest.dropFirst()
                    } else {
                        break
                    }
                }
            } else {
                let token = rest.prefix { !whitespace.contains($0) && !"(),'".contains($0) }
                argument = String(token)
                rest = rest.dropFirst(token.count)
            }
            arguments.append(argument)
            rest = rest.drop { whitespace.contains($0) }
            switch rest.first {
            case ")": return arguments
            case ",": rest = rest.dropFirst()
            default: return nil
            }
        }
    }

    struct Constraint: Equatable {
        let name: String
        let negated: Bool
    }

    static func constraints(_ list: String) -> [Constraint] {
        list.split(separator: "#").compactMap { token in
            var name = token.trimmingCharacters(in: .whitespacesAndNewlines)[...]
            let negated = name.first == "!"
            if negated { name = name.dropFirst() }
            let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
            return trimmed.isEmpty ? nil : Constraint(name: trimmed, negated: negated)
        }
    }

    static func passes(_ constraints: [Constraint], language: String?, all: Set<String>) -> Bool {
        guard !constraints.isEmpty, let language else { return true }
        if all.count > 1, all.allSatisfy({ name in constraints.contains { !$0.negated && $0.name == name } }) { return true }
        return constraints.contains { $0.negated ? $0.name != language : $0.name == language }
    }

    static func passesArchitecture(_ constraints: [Constraint]) -> Bool {
        guard !constraints.isEmpty else { return true }
        if constraints.count == 1, constraints[0].negated { return constraints[0].name != "64" }
        return constraints.contains { !$0.negated && $0.name == "64" }
    }

    private static func md5(_ text: String) -> [UInt8]? {
        let digits = Array(text.utf8)
        guard digits.count == 32 else { return nil }
        var bytes: [UInt8] = []
        for index in stride(from: 0, to: 32, by: 2) {
            guard let byte = UInt8(String(decoding: digits[index..<(index + 2)], as: UTF8.self), radix: 16) else { return nil }
            bytes.append(byte)
        }
        return bytes
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
