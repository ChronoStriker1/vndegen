import Foundation

nonisolated enum NSISPayload {
    static let signature: [UInt8] = [0x37, 0x7a, 0xbc, 0xaf, 0x27, 0x1c]
    private static let maxBlocks = 64

    static func offset(in url: URL) -> Int64? {
        PEOverlay.read(url) { (pe: PEOverlay) -> Int64? in
            guard let first = pe.bytes(at: pe.start, count: 28), first.u32(4) == 0xdead_beef,
                  first[8..<20].elementsEqual(Array("NullsoftInst".utf8)) else { return nil }
            let end = min(pe.size, pe.start + UInt64(first.u32(24)))
            var position = pe.start + 28
            for _ in 0..<maxBlocks {
                guard position + 4 <= end, let length = pe.bytes(at: position, count: 4).map({ $0.u32(0) }) else { return nil }
                let data = position + 4
                let blockSize = UInt64(length & 0x7fff_ffff)
                guard data + blockSize <= end else { return nil }
                if length & 0x8000_0000 == 0, blockSize >= UInt64(signature.count), pe.bytes(at: data, count: signature.count) == signature {
                    return Int64(data)
                }
                position = data + blockSize
            }
            return nil
        }
    }
}
