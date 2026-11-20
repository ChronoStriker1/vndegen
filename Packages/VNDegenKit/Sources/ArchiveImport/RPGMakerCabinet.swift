import Foundation

nonisolated enum RPGMakerCabinet {
    private static let marker = "Software\\Enterbrain\\RPGVX".utf16.flatMap { [UInt8($0 & 0xff), UInt8($0 >> 8)] }
    private static let maxStub = 8 << 20

    static func isPacked(_ url: URL) -> Bool { offset(in: url) != nil }

    static func offset(in url: URL) -> Int64? {
        PEOverlay.read(url) { (pe: PEOverlay) -> Int64? in
            guard pe.start <= UInt64(maxStub), let header = pe.bytes(at: pe.start, count: 12), header.starts(with: Array("MSCF".utf8)),
                  UInt64(header.u32(8)) == pe.size - pe.start, let stub = pe.bytes(at: 0, count: Int(pe.start)),
                  stub.firstRange(of: marker) != nil else { return nil }
            return Int64(pe.start)
        }
    }
}
