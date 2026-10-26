/// MD4 (RFC 1320). rsync protocol 27 checksums every transferred file with it, and CryptoKit has no MD4.
struct MD4 {
    private var state: (UInt32, UInt32, UInt32, UInt32) = (0x6745_2301, 0xEFCD_AB89, 0x98BA_DCFE, 0x1032_5476)
    private var pending: [UInt8] = []
    private var length: UInt64 = 0

    mutating func update(_ bytes: some Collection<UInt8>) {
        length += UInt64(bytes.count)
        pending.append(contentsOf: bytes)
        let whole = pending.count / 64 * 64
        guard whole > 0 else { return }
        pending.withUnsafeBufferPointer { buffer in
            for offset in stride(from: 0, to: whole, by: 64) { compress(UnsafeBufferPointer(rebasing: buffer[offset ..< offset + 64])) }
        }
        pending.removeFirst(whole)
    }

    mutating func update(_ value: Int32) {
        withUnsafeBytes(of: value.littleEndian) { update(Array($0)) }
    }

    mutating func finalize() -> [UInt8] {
        let bits = length * 8
        var tail: [UInt8] = [0x80]
        tail += [UInt8](repeating: 0, count: (55 - Int(length % 64) + 64) % 64)
        tail += withUnsafeBytes(of: bits.littleEndian) { Array($0) }
        update(tail)
        return [state.0, state.1, state.2, state.3].flatMap { word in withUnsafeBytes(of: word.littleEndian) { Array($0) } }
    }

    static func hash(_ bytes: some Collection<UInt8>) -> [UInt8] {
        var md4 = MD4()
        md4.update(bytes)
        return md4.finalize()
    }

    private mutating func compress(_ block: UnsafeBufferPointer<UInt8>) {
        var x = [UInt32](repeating: 0, count: 16)
        for i in 0 ..< 16 {
            x[i] = UInt32(block[i * 4]) | UInt32(block[i * 4 + 1]) << 8 | UInt32(block[i * 4 + 2]) << 16 | UInt32(block[i * 4 + 3]) << 24
        }
        var (a, b, c, d) = state
        func rotl(_ v: UInt32, _ s: UInt32) -> UInt32 { v << s | v >> (32 - s) }
        func f(_ x: UInt32, _ y: UInt32, _ z: UInt32) -> UInt32 { x & y | ~x & z }
        func g(_ x: UInt32, _ y: UInt32, _ z: UInt32) -> UInt32 { x & y | x & z | y & z }
        func h(_ x: UInt32, _ y: UInt32, _ z: UInt32) -> UInt32 { x ^ y ^ z }

        for i in [0, 4, 8, 12] {
            a = rotl(a &+ f(b, c, d) &+ x[i], 3)
            d = rotl(d &+ f(a, b, c) &+ x[i + 1], 7)
            c = rotl(c &+ f(d, a, b) &+ x[i + 2], 11)
            b = rotl(b &+ f(c, d, a) &+ x[i + 3], 19)
        }
        for i in [0, 1, 2, 3] {
            a = rotl(a &+ g(b, c, d) &+ x[i] &+ 0x5A82_7999, 3)
            d = rotl(d &+ g(a, b, c) &+ x[i + 4] &+ 0x5A82_7999, 5)
            c = rotl(c &+ g(d, a, b) &+ x[i + 8] &+ 0x5A82_7999, 9)
            b = rotl(b &+ g(c, d, a) &+ x[i + 12] &+ 0x5A82_7999, 13)
        }
        for i in [0, 2, 1, 3] {
            a = rotl(a &+ h(b, c, d) &+ x[i] &+ 0x6ED9_EBA1, 3)
            d = rotl(d &+ h(a, b, c) &+ x[i + 8] &+ 0x6ED9_EBA1, 9)
            c = rotl(c &+ h(d, a, b) &+ x[i + 4] &+ 0x6ED9_EBA1, 11)
            b = rotl(b &+ h(c, d, a) &+ x[i + 12] &+ 0x6ED9_EBA1, 15)
        }
        state = (state.0 &+ a, state.1 &+ b, state.2 &+ c, state.3 &+ d)
    }
}
