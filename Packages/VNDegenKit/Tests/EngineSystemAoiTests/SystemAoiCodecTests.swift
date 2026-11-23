import CSystemAoi
import Foundation
import Testing

private func u32(_ v: Int) -> [UInt8] { withUnsafeBytes(of: UInt32(v).littleEndian) { Array($0) } }

private func file(total: Int, flags: Int = 0, method: UInt8? = nil, body: [UInt8]) -> [UInt8] {
    let block = method.map { u32(body.count) + [$0] } ?? []
    let size = 16 + block.count + body.count
    return u32(total) + u32(size) + u32(total) + u32(flags) + block + body
}

private func unpack(_ bytes: [UInt8]) -> [UInt8]? {
    var size = 0
    guard let out = bytes.withUnsafeBufferPointer({ vnsystemaoi_unpack($0.baseAddress, $0.count, &size) }) else { return nil }
    defer { free(out) }
    return Array(UnsafeBufferPointer(start: out, count: size))
}

private func pack(_ bytes: [UInt8]) -> [UInt8] {
    var size = 0
    let out = bytes.withUnsafeBufferPointer { vnsystemaoi_pack($0.baseAddress, $0.count, &size) }!
    defer { free(out) }
    return Array(UnsafeBufferPointer(start: out, count: size))
}

@Suite struct SystemAoiCodecTests {
    @Test func unpacksLiteralsAndAMatch() {
        let body: [UInt8] = [3] + Array("abc".utf8) + [0, 3, 0, 6]
        #expect(unpack(file(total: 9, method: 0x02, body: body)) == Array("abcabcabc".utf8))
    }

    @Test func unpacksRunsOfEachWidth() {
        let body: [UInt8] = [1, 5, 0x78, 0, 2, 0x79, 0x7a, 3, 2, 0x70, 0x71, 0x72]
        #expect(unpack(file(total: 13, method: 0x01, body: body)) == Array("xxxxxyzpqrpqr".utf8))
    }

    @Test func unpacksAStoredFile() {
        #expect(unpack(file(total: 3, flags: 1, body: Array("abc".utf8))) == Array("abc".utf8))
    }

    @Test func rejectsAWrongFileSize() {
        var bytes = file(total: 9, method: 0x02, body: [3] + Array("abc".utf8) + [0, 3, 0, 6])
        bytes[4] += 1
        #expect(unpack(bytes) == nil)
    }

    @Test func packsZerosAsRunsLeavingTheLastByteLiteral() {
        let runs: [UInt8] = [1, 255, 0, 1, 255, 0, 1, 255, 0, 1, 234, 0, 0, 1, 0]
        #expect(pack([UInt8](repeating: 0, count: 1000)) == u32(1000) + u32(36) + u32(1000) + u32(0) + u32(15) + [0x01] + runs)
    }

    @Test func roundTripsMixedData() {
        var seed: UInt32 = 12345
        var data: [UInt8] = []
        for i in 0..<20000 {
            seed = seed &* 1_103_515_245 &+ 12345
            data.append(i % 7 == 0 ? UInt8(truncatingIfNeeded: seed >> 16) : UInt8(i % 251 / 3))
        }
        let packed = pack(data)
        #expect(packed.count < data.count)
        #expect(unpack(packed) == data)
    }
}
