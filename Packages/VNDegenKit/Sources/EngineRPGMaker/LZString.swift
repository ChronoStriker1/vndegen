import Foundation

/// The base64 form of lz-string, which RPG Maker MV stores its save files in. Strings are handled as UTF-16 code units,
/// as JavaScript sees them. MV ships lz-string 1.3: compressing writes the text its `compressToBase64` writes, and
/// decompressing follows lz-string 1.4.4, which reads both versions' output.
nonisolated enum LZString {
    private static let alphabet = Array("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=".utf16)
    private static let values: [UInt16: Int] = Dictionary(uniqueKeysWithValues: alphabet.enumerated().map { ($1, $0) })

    /// lz-string 1.3 writes the 16-bit form's bytes in base64. lz-string 1.4's 6-bit form can end in a character that
    /// 1.3 drops, so 1.3 can't always read it.
    static func compressToBase64(_ text: String) -> String {
        var bytes = Data()
        for unit in compress(Array(text.utf16), bitsPerChar: 16, character: { UInt16($0) }) {
            bytes.append(UInt8(unit >> 8))
            bytes.append(UInt8(unit & 0xff))
        }
        return bytes.base64EncodedString()
    }

    /// Nil when `text` is not lz-string base64.
    static func decompressFromBase64(_ text: String) -> String? {
        let input = Array(text.utf16)
        guard !input.isEmpty else { return nil }
        var digits: [Int] = []
        digits.reserveCapacity(input.count)
        for unit in input {
            guard let value = values[unit] else { return nil }
            digits.append(value)
        }
        return decompress(digits, resetValue: 32).map { String(decoding: $0, as: UTF16.self) }
    }

    private static func compress(_ input: [UInt16], bitsPerChar: Int, character: (Int) -> UInt16) -> [UInt16] {
        var dictionary: [[UInt16]: Int] = [:]
        var toCreate: Set<[UInt16]> = []
        var w: [UInt16] = []
        var enlargeIn = 2, dictSize = 3, numBits = 2
        var out: [UInt16] = []
        var value = 0, position = 0

        func write(_ bits: Int, count: Int) {
            var bits = bits
            for _ in 0..<count {
                value = (value << 1) | (bits & 1)
                if position == bitsPerChar - 1 {
                    position = 0
                    out.append(character(value))
                    value = 0
                } else {
                    position += 1
                }
                bits >>= 1
            }
        }
        func shrink() {
            enlargeIn -= 1
            if enlargeIn == 0 {
                enlargeIn = 1 << numBits
                numBits += 1
            }
        }
        // Writes the code for `w`: a new character as a literal the first time, otherwise its dictionary entry.
        func emit(_ w: [UInt16]) {
            if toCreate.contains(w) {
                let unit = Int(w[0])
                if unit < 256 {
                    write(0, count: numBits)
                    write(unit, count: 8)
                } else {
                    write(1, count: numBits)
                    write(unit, count: 16)
                }
                shrink()
                toCreate.remove(w)
            } else {
                write(dictionary[w]!, count: numBits)
            }
            shrink()
        }

        for unit in input {
            let c = [unit]
            if dictionary[c] == nil {
                dictionary[c] = dictSize
                dictSize += 1
                toCreate.insert(c)
            }
            let wc = w + c
            if dictionary[wc] != nil {
                w = wc
            } else {
                emit(w)
                dictionary[wc] = dictSize
                dictSize += 1
                w = c
            }
        }
        if !w.isEmpty { emit(w) }
        write(2, count: numBits)
        // Pads the last character with zero bits.
        while true {
            value <<= 1
            if position == bitsPerChar - 1 {
                out.append(character(value))
                break
            }
            position += 1
        }
        return out
    }

    private static func decompress(_ input: [Int], resetValue: Int) -> [UInt16]? {
        var dictionary: [[UInt16]] = [[0], [1], [2]]
        var enlargeIn = 4, numBits = 3
        var value = input[0], position = resetValue, index = 1

        func read(_ count: Int) -> Int {
            var bits = 0, power = 1
            for _ in 0..<count {
                let bit = value & position
                position >>= 1
                if position == 0 {
                    position = resetValue
                    value = index < input.count ? input[index] : 0
                    index += 1
                }
                if bit != 0 { bits |= power }
                power <<= 1
            }
            return bits
        }

        let first: [UInt16]
        switch read(2) {
        case 0: first = [UInt16(read(8))]
        case 1: first = [UInt16(read(16))]
        default: return []
        }
        dictionary.append(first)
        var w = first, result = first
        while true {
            guard index <= input.count else { return nil }
            var code = read(numBits)
            switch code {
            case 0, 1:
                dictionary.append([UInt16(read(code == 0 ? 8 : 16))])
                code = dictionary.count - 1
                enlargeIn -= 1
            case 2:
                return result
            default:
                break
            }
            if enlargeIn == 0 {
                enlargeIn = 1 << numBits
                numBits += 1
            }
            let entry: [UInt16]
            if code < dictionary.count {
                entry = dictionary[code]
            } else if code == dictionary.count {
                entry = w + [w[0]]
            } else {
                return nil
            }
            result += entry
            dictionary.append(w + [entry[0]])
            enlargeIn -= 1
            w = entry
            if enlargeIn == 0 {
                enlargeIn = 1 << numBits
                numBits += 1
            }
        }
    }
}
