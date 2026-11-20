import Foundation

nonisolated final class LZXDecoder {
    struct Damaged: Error, Equatable {
        let detail: String
    }

    static let frameSize = 32768

    private static let slotCounts = [15: 30, 16: 32, 17: 34, 18: 36, 19: 38, 20: 42, 21: 50]
    private static let footerBits: [Int] = (0..<50).map { $0 < 4 ? 0 : min($0 / 2 - 1, 17) }
    private static let positionBase: [Int] = (0..<50).reduce(into: [Int]()) { bases, slot in
        bases.append(slot == 0 ? 0 : bases[slot - 1] + (1 << footerBits[slot - 1]))
    }

    private enum Block { case none, verbatim, aligned, uncompressed }

    private let windowMask: Int
    private let window: UnsafeMutablePointer<UInt8>
    private var position = 0
    private var produced = 0
    private var frame = 0

    private var mainLengths: [UInt8]
    private var lengthLengths = [UInt8](repeating: 0, count: 249)
    private let main: Huffman
    private let length = Huffman(symbols: 249, tableBits: 12)
    private let aligned = Huffman(symbols: 8, tableBits: 7)
    private let pre = Huffman(symbols: 20, tableBits: 8)

    private var headerRead = false
    private var translates = false
    private var translationSize: Int32 = 0
    private var block = Block.none
    private var blockLeft = 0
    private var blockOdd = false
    private var padPending = false
    private var r0 = 1, r1 = 1, r2 = 1
    private var matchLeft = 0
    private var matchOffset = 0

    private var input = UnsafeRawBufferPointer(start: nil, count: 0)
    private var inPos = 0
    private var bits: UInt64 = 0
    private var bitCount = 0

    init(windowBits: Int) throws(Damaged) {
        guard let slots = Self.slotCounts[windowBits] else { throw Damaged(detail: "LZX window of 2^\(windowBits) bytes") }
        windowMask = (1 << windowBits) - 1
        window = .allocate(capacity: 1 << windowBits)
        window.initialize(repeating: 0, count: 1 << windowBits)
        mainLengths = [UInt8](repeating: 0, count: 256 + slots * 8)
        main = Huffman(symbols: 256 + slots * 8, tableBits: 12)
    }

    deinit { window.deallocate() }

    func decode(_ data: UnsafeRawBufferPointer, count: Int, into out: UnsafeMutablePointer<UInt8>) throws(Damaged) {
        guard count > 0, count <= Self.frameSize else { throw Damaged(detail: "an LZX frame of \(count) bytes") }
        input = data
        inPos = 0
        bits = 0
        bitCount = 0
        if padPending {
            inPos = 1
            padPending = false
        }
        if !headerRead {
            headerRead = true
            if try read(1) == 1 {
                translates = true
                translationSize = Int32(bitPattern: UInt32(try read(16)) << 16 | UInt32(try read(16)))
            }
        }
        var done = 0
        while done < count {
            if matchLeft > 0 {
                done += copyMatch(into: out + done, limit: count - done)
                continue
            }
            if blockLeft <= 0 { try readBlockHeader() }
            switch block {
            case .uncompressed:
                let n = min(blockLeft, count - done, input.count - inPos)
                guard n > 0 else { throw Damaged(detail: "an uncompressed LZX block runs past its data") }
                for i in 0..<n {
                    let byte = input[inPos + i]
                    window[position] = byte
                    out[done + i] = byte
                    position = (position + 1) & windowMask
                }
                inPos += n
                done += n
                produced += n
                blockLeft -= n
                if blockLeft == 0 {
                    if blockOdd {
                        if inPos < input.count { inPos += 1 } else { padPending = true }
                    }
                    bits = 0
                    bitCount = 0
                }
            case .verbatim, .aligned:
                try decodeSymbols(into: out, done: &done, count: count)
            case .none:
                throw Damaged(detail: "no LZX block")
            }
        }
        if translates, frame < 32768, count > 10 { translate(out, count: count) }
        frame += 1
    }

    private func decodeSymbols(into out: UnsafeMutablePointer<UInt8>, done: inout Int, count: Int) throws(Damaged) {
        let isAligned = block == .aligned
        while done < count, blockLeft > 0, matchLeft == 0 {
            let symbol = try decodeSymbol(main)
            if symbol < 256 {
                let byte = UInt8(symbol)
                window[position] = byte
                out[done] = byte
                position = (position + 1) & windowMask
                done += 1
                produced += 1
                blockLeft -= 1
                continue
            }
            let header = symbol - 256
            var matchLength = header & 7
            if matchLength == 7 { matchLength += try decodeSymbol(length) }
            matchLength += 2
            let slot = header >> 3
            var offset: Int
            switch slot {
            case 0:
                offset = r0
            case 1:
                offset = r1
                r1 = r0
                r0 = offset
            case 2:
                offset = r2
                r2 = r0
                r0 = offset
            default:
                let extra = Self.footerBits[slot]
                var formatted = Self.positionBase[slot]
                if isAligned, extra >= 3 {
                    formatted += Int(try read(extra - 3)) << 3
                    formatted += try decodeSymbol(aligned)
                } else if extra > 0 {
                    formatted += Int(try read(extra))
                }
                offset = formatted - 2
                r2 = r1
                r1 = r0
                r0 = offset
            }
            guard offset > 0, offset <= produced, offset <= windowMask + 1 else {
                throw Damaged(detail: "an LZX match \(offset) bytes back, with \(produced) written")
            }
            matchLeft = matchLength
            matchOffset = offset
            blockLeft -= matchLength
            done += copyMatch(into: out + done, limit: count - done)
        }
    }

    private func copyMatch(into out: UnsafeMutablePointer<UInt8>, limit: Int) -> Int {
        let n = min(matchLeft, limit)
        var from = (position - matchOffset) & windowMask
        for i in 0..<n {
            let byte = window[from]
            window[position] = byte
            out[i] = byte
            position = (position + 1) & windowMask
            from = (from + 1) & windowMask
        }
        matchLeft -= n
        produced += n
        return n
    }

    private func readBlockHeader() throws(Damaged) {
        let overrun = blockLeft
        let type = try read(3)
        let size = Int(try read(8)) << 16 | Int(try read(16))
        guard size > 0 else { throw Damaged(detail: "an empty LZX block") }
        blockLeft = size + overrun
        switch type {
        case 1, 2:
            block = type == 1 ? .verbatim : .aligned
            if type == 2 {
                var lengths = [UInt8](repeating: 0, count: 8)
                for i in 0..<8 { lengths[i] = UInt8(try read(3)) }
                try aligned.build(lengths)
            }
            try readLengths(&mainLengths, 0, 256)
            try readLengths(&mainLengths, 256, mainLengths.count)
            try main.build(mainLengths)
            try readLengths(&lengthLengths, 0, 249)
            try length.build(lengthLengths)
        case 3:
            block = .uncompressed
            blockOdd = size & 1 == 1
            if bitCount & 15 != 0 {
                bitCount &= ~15
            } else {
                try ensure(16)
                bitCount -= 16
            }
            inPos -= bitCount / 8
            bits = 0
            bitCount = 0
            guard inPos + 12 <= input.count else { throw Damaged(detail: "an uncompressed LZX block's header runs past its data") }
            var repeats = [Int]()
            for i in 0..<3 {
                let value = Int(input.loadUnaligned(fromByteOffset: inPos + 4 * i, as: UInt32.self).littleEndian)
                guard value > 0 else { throw Damaged(detail: "a repeated LZX offset of 0") }
                repeats.append(value)
            }
            (r0, r1, r2) = (repeats[0], repeats[1], repeats[2])
            inPos += 12
        default:
            throw Damaged(detail: "an LZX block of type \(type)")
        }
    }

    private func readLengths(_ lengths: inout [UInt8], _ first: Int, _ last: Int) throws(Damaged) {
        var preLengths = [UInt8](repeating: 0, count: 20)
        for i in 0..<20 { preLengths[i] = UInt8(try read(4)) }
        try pre.build(preLengths)
        var i = first
        while i < last {
            let code = try decodeSymbol(pre)
            switch code {
            case 17, 18:
                let run = code == 17 ? Int(try read(4)) + 4 : Int(try read(5)) + 20
                guard i + run <= last else { throw Damaged(detail: "LZX code lengths run past their tree") }
                for j in i..<(i + run) { lengths[j] = 0 }
                i += run
            case 19:
                let run = Int(try read(1)) + 4
                guard i + run <= last else { throw Damaged(detail: "LZX code lengths run past their tree") }
                let next = try decodeSymbol(pre)
                guard next <= 16 else { throw Damaged(detail: "an LZX length code of \(next) after a run") }
                let value = UInt8((Int(lengths[i]) - next + 17) % 17)
                for j in i..<(i + run) { lengths[j] = value }
                i += run
            default:
                lengths[i] = UInt8((Int(lengths[i]) - code + 17) % 17)
                i += 1
            }
        }
    }

    private func translate(_ out: UnsafeMutablePointer<UInt8>, count: Int) {
        let start = Int32(frame * Self.frameSize)
        var i = 0
        while i < count - 10 {
            guard out[i] == 0xE8 else {
                i += 1
                continue
            }
            let current = start + Int32(i)
            let value = Int32(bitPattern: UInt32(out[i + 1]) | UInt32(out[i + 2]) << 8 | UInt32(out[i + 3]) << 16 | UInt32(out[i + 4]) << 24)
            if value >= -current, value < translationSize {
                let relative = UInt32(bitPattern: value >= 0 ? value - current : value + translationSize)
                for k in 0..<4 { out[i + 1 + k] = UInt8(truncatingIfNeeded: relative >> (8 * k)) }
            }
            i += 5
        }
    }

    @inline(__always)
    private func ensure(_ n: Int) throws(Damaged) {
        while bitCount < n {
            var word: UInt64 = 0
            if inPos + 1 < input.count {
                word = UInt64(input[inPos]) | UInt64(input[inPos + 1]) << 8
            } else if inPos < input.count {
                word = UInt64(input[inPos])
            } else if inPos > input.count + 8 {
                throw Damaged(detail: "LZX data runs past its block")
            }
            inPos += 2
            bits = bits << 16 | word
            bitCount += 16
        }
    }

    @inline(__always)
    private func read(_ n: Int) throws(Damaged) -> UInt32 {
        guard n > 0 else { return 0 }
        try ensure(n)
        bitCount -= n
        return UInt32(truncatingIfNeeded: bits >> UInt64(bitCount)) & UInt32((1 << n) - 1)
    }

    @inline(__always)
    private func decodeSymbol(_ tree: Huffman) throws(Damaged) -> Int {
        try ensure(16)
        let peek = Int(truncatingIfNeeded: bits >> UInt64(bitCount - 16)) & 0xFFFF
        guard let (symbol, length) = tree.lookup(peek) else { throw Damaged(detail: "an LZX code that no tree holds") }
        bitCount -= length
        return symbol
    }
}

nonisolated private final class Huffman {
    private let symbols: Int
    private let tableBits: Int
    private var table: [UInt32]
    private var counts = [Int](repeating: 0, count: 17)
    private var firstCode = [Int](repeating: 0, count: 17)
    private var firstIndex = [Int](repeating: 0, count: 17)
    private var sorted: [Int]

    init(symbols: Int, tableBits: Int) {
        self.symbols = symbols
        self.tableBits = tableBits
        table = [UInt32](repeating: 0, count: 1 << tableBits)
        sorted = [Int](repeating: 0, count: symbols)
    }

    func build(_ lengths: [UInt8]) throws(LZXDecoder.Damaged) {
        counts = [Int](repeating: 0, count: 17)
        for length in lengths where length > 0 { counts[Int(length)] += 1 }
        var room = 1
        for bitsUsed in 1...16 {
            room = room * 2 - counts[bitsUsed]
            guard room >= 0 else { throw LZXDecoder.Damaged(detail: "an LZX tree with too many codes") }
        }
        var code = 0, index = 0
        for bitsUsed in 1...16 {
            code = (code + (bitsUsed > 1 ? counts[bitsUsed - 1] : 0)) << (bitsUsed > 1 ? 1 : 0)
            firstCode[bitsUsed] = code
            firstIndex[bitsUsed] = index
            index += counts[bitsUsed]
        }
        var next = firstIndex
        for (symbol, length) in lengths.enumerated() where length > 0 {
            sorted[next[Int(length)]] = symbol
            next[Int(length)] += 1
        }
        for i in table.indices { table[i] = 0 }
        for bitsUsed in 1...min(16, tableBits) where counts[bitsUsed] > 0 {
            for k in 0..<counts[bitsUsed] {
                let symbol = sorted[firstIndex[bitsUsed] + k]
                let start = (firstCode[bitsUsed] + k) << (tableBits - bitsUsed)
                let entry = UInt32(bitsUsed) << 16 | UInt32(symbol)
                for slot in start..<(start + (1 << (tableBits - bitsUsed))) { table[slot] = entry }
            }
        }
    }

    @inline(__always)
    func lookup(_ peek: Int) -> (Int, Int)? {
        let entry = table[peek >> (16 - tableBits)]
        if entry != 0 { return (Int(entry & 0xFFFF), Int(entry >> 16)) }
        guard tableBits < 16 else { return nil }
        for bitsUsed in (tableBits + 1)...16 {
            let code = peek >> (16 - bitsUsed)
            let k = code - firstCode[bitsUsed]
            if k >= 0, k < counts[bitsUsed] { return (sorted[firstIndex[bitsUsed] + k], bitsUsed) }
        }
        return nil
    }
}
