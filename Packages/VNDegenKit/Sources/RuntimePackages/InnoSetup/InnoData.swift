// Inno Setup 5 file data reader for RuntimePackages.
//
// Ported from innoextract (https://github.com/dscharrer/innoextract): the slice and chunk layout and the executable
// filters follow its stream code. Altered from the original: rewritten in Swift with streaming buffers, no encryption.
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

/// Positioned reads from a file that must not run short.
final class RandomAccessFile {
    let url: URL
    let size: UInt64
    private let handle: FileHandle

    init(url: URL) throws {
        self.url = url
        handle = try FileHandle(forReadingFrom: url)
        size = try handle.seekToEnd()
    }

    deinit { try? handle.close() }

    func read(at offset: UInt64, count: Int) throws -> [UInt8] {
        guard offset <= size, UInt64(count) <= size - offset else {
            throw RuntimeInstallError.corrupt("\(url.lastPathComponent) ends early")
        }
        try handle.seek(toOffset: offset)
        let data = try handle.read(upToCount: count) ?? Data()
        guard data.count == count else { throw RuntimeInstallError.corrupt("\(url.lastPathComponent) ends early") }
        return [UInt8](data)
    }

    /// Reads up to `buffer.count` bytes at `offset`.
    func read(into buffer: UnsafeMutableRawBufferPointer, at offset: UInt64) throws -> Int {
        try handle.seek(toOffset: offset)
        let data = try handle.read(upToCount: buffer.count) ?? Data()
        _ = data.copyBytes(to: buffer.bindMemory(to: UInt8.self))
        return data.count
    }
}

/// The installer's file data: embedded in the setup program after `dataOffset`, or split into slice files
/// (`setup-1.bin`, `setup-2.bin`, ...) that each start with a 12-byte header.
final class InnoSliceReader {
    private let setup: RandomAccessFile
    private let dataOffset: UInt64
    private let slicesPerDisk: Int
    private let baseNames: [String]
    private var current: (index: Int, file: RandomAccessFile, end: UInt64)?
    private var position: UInt64 = 0

    init(setup: RandomAccessFile, headers: InnoHeaders) {
        self.setup = setup
        dataOffset = headers.offsets.dataOffset
        slicesPerDisk = max(1, headers.slicesPerDisk)
        let setupName = setup.url.deletingPathExtension().lastPathComponent
        let headerName = headers.baseFilename.replacing("/", with: "_").replacing("\\", with: "_")
        baseNames = [setupName, headerName].filter { !$0.isEmpty }
    }

    func seek(slice: Int, offset: UInt64) throws {
        try open(slice: slice)
        position = offset + dataOffset
        guard let current, position <= current.end else { throw RuntimeInstallError.corrupt("chunk outside its slice") }
    }

    /// Reads up to `buffer.count` bytes, moving on to the next slice file at the end of one.
    func read(into buffer: UnsafeMutableRawBufferPointer) throws -> Int {
        guard var current else { return 0 }
        if position == current.end {
            guard dataOffset == 0 else { return 0 }
            try open(slice: current.index + 1)
            current = self.current!
            position = 12
        }
        let count = Int(min(UInt64(buffer.count), current.end - position))
        let read = try current.file.read(into: UnsafeMutableRawBufferPointer(rebasing: buffer[0..<count]), at: position)
        position += UInt64(read)
        return read
    }

    private func open(slice: Int) throws {
        if current?.index == slice { return }
        if dataOffset != 0 {
            guard slice == 0 else { throw RuntimeInstallError.corrupt("embedded data has no slice \(slice)") }
            current = (0, setup, setup.size)
            return
        }
        let suffix = slicesPerDisk == 1
            ? "\(slice + 1)"
            : "\(slice / slicesPerDisk + 1)\(Character(UnicodeScalar(UInt8(ascii: "a") + UInt8(slice % slicesPerDisk))))"
        let folder = setup.url.deletingLastPathComponent()
        let names = baseNames.map { "\($0)-\(suffix).bin" }
        let present = (try? FileManager.default.contentsOfDirectory(atPath: folder.path)) ?? []
        guard let name = names.lazy.compactMap({ name in present.first { $0.lowercased() == name.lowercased() } }).first
        else { throw RuntimeInstallError.corrupt("missing data file \(names[0])") }
        let file = try RandomAccessFile(url: folder.appending(path: name))
        var header = ByteReader(try file.read(at: 0, count: 12))
        let magic = Array(try header.take(8))
        guard magic == Array("idska16".utf8) + [0x1a] || magic == Array("idska32".utf8) + [0x1a] else {
            throw RuntimeInstallError.corrupt("\(name) is not an Inno Setup data file")
        }
        let end = UInt64(try header.u32())
        guard end >= 12, end <= file.size else { throw RuntimeInstallError.corrupt("bad size in \(name)") }
        current = (slice, file, end)
    }
}

/// One chunk's decompressed bytes. A chunk is a `zlb\x1a` marker and `size` compressed bytes holding one or more files.
final class InnoChunkStream {
    private let slices: InnoSliceReader
    private let decoder: Decompressor
    private var remaining: UInt64
    private var input = [UInt8](repeating: 0, count: 1 << 18)
    private var inputStart = 0
    private var inputEnd = 0
    private var ended = false

    init(slices: InnoSliceReader, entry: InnoHeaders.DataEntry) throws {
        guard !entry.encrypted else { throw RuntimeInstallError.unsupportedInstaller("encrypted files are not supported") }
        self.slices = slices
        try slices.seek(slice: entry.firstSlice, offset: entry.chunkOffset)
        var magic = [UInt8](repeating: 0, count: 4)
        let count = try magic.withUnsafeMutableBytes { try slices.read(into: $0) }
        guard count == 4, magic == Array("zlb".utf8) + [0x1a] else { throw RuntimeInstallError.corrupt("bad chunk marker") }
        remaining = entry.chunkSize
        switch entry.compression {
        case .stored: decoder = StoredDecoder()
        case .zlib: decoder = try ZlibDecoder()
        case .bzip2: decoder = try BZip2Decoder()
        case .lzma1: decoder = LZMA1Decoder()
        case .lzma2: throw RuntimeInstallError.unsupportedInstaller("LZMA2 compression is not supported")
        }
    }

    /// Fills `buffer` completely unless the chunk runs out, which means the installer is damaged.
    func read(into buffer: UnsafeMutableRawBufferPointer) throws {
        var filled = 0
        while filled < buffer.count {
            if inputStart == inputEnd, remaining > 0 {
                let want = Int(min(UInt64(input.count), remaining))
                let count = try input.withUnsafeMutableBytes {
                    try slices.read(into: UnsafeMutableRawBufferPointer(rebasing: $0[0..<want]))
                }
                guard count > 0 else { throw RuntimeInstallError.corrupt("chunk data ends early") }
                (inputStart, inputEnd) = (0, count)
                remaining -= UInt64(count)
            }
            guard !ended else { throw RuntimeInstallError.corrupt("chunk holds less data than its files") }
            let (used, produced, finished) = try input.withUnsafeBytes { input in
                try decoder.decode(UnsafeRawBufferPointer(rebasing: input[inputStart..<inputEnd]),
                                   into: UnsafeMutableRawBufferPointer(rebasing: buffer[filled...]))
            }
            inputStart += used
            filled += produced
            ended = finished
            if used == 0, produced == 0, inputStart == inputEnd, remaining == 0 {
                throw RuntimeInstallError.corrupt("chunk holds less data than its files")
            }
        }
    }

    func skip(_ count: UInt64) throws {
        var left = count
        var scratch = [UInt8](repeating: 0, count: 1 << 16)
        while left > 0 {
            let step = Int(min(UInt64(scratch.count), left))
            try scratch.withUnsafeMutableBytes { try read(into: UnsafeMutableRawBufferPointer(rebasing: $0[0..<step])) }
            left -= UInt64(step)
        }
    }
}

/// Undoes Inno Setup's x86 CALL and JMP address transform for files flagged "call instruction optimized". Output is
/// as long as the input, but the 5.2.0 form holds back up to four address bytes until it has seen all of them.
struct InnoExecutableDecoder {
    private let kind: InnoHeaders.ExecutableFilter
    private var offset: UInt32 = 0
    // 4108 form
    private var address: UInt32 = 0
    private var addressBytesLeft = 0
    private var addressOffset: UInt32 = 5
    // 5200 form
    private var pending: [UInt8] = []
    private var collecting = false

    init(_ kind: InnoHeaders.ExecutableFilter) { self.kind = kind }

    mutating func process(_ input: [UInt8]) -> [UInt8] {
        switch kind {
        case .noFilter:
            return input
        case .v4108:
            return input.map { byte in
                defer { addressOffset &+= 1 }
                if addressBytesLeft == 0 {
                    if byte == 0xe8 || byte == 0xe9 {
                        address = ~addressOffset &+ 1
                        addressBytesLeft = 4
                    }
                    return byte
                }
                address &+= UInt32(byte)
                let decoded = UInt8(truncatingIfNeeded: address)
                address >>= 8
                addressBytesLeft -= 1
                return decoded
            }
        case .v5200(let flipHighByte):
            var output: [UInt8] = []
            output.reserveCapacity(input.count + 4)
            for byte in input {
                offset &+= 1
                if !collecting {
                    output.append(byte)
                    // Instructions that would straddle a 64 KiB block are left alone.
                    if (byte == 0xe8 || byte == 0xe9), 0x10000 - ((offset - 1) % 0x10000) >= 5 { collecting = true }
                    continue
                }
                pending.append(byte)
                guard pending.count == 4 else { continue }
                if pending[3] == 0x00 || pending[3] == 0xff {
                    let relative = (UInt32(pending[0]) | UInt32(pending[1]) << 8 | UInt32(pending[2]) << 16)
                        &- (offset & 0xff_ffff)
                    pending[0] = UInt8(truncatingIfNeeded: relative)
                    pending[1] = UInt8(truncatingIfNeeded: relative >> 8)
                    pending[2] = UInt8(truncatingIfNeeded: relative >> 16)
                    if flipHighByte, relative & 0x80_0000 != 0 { pending[3] = ~pending[3] }
                }
                output += pending
                pending.removeAll(keepingCapacity: true)
                collecting = false
            }
            return output
        }
    }

    /// The bytes held back when the file ended partway through an address.
    mutating func finish() -> [UInt8] {
        defer { pending.removeAll() }
        return pending
    }
}
