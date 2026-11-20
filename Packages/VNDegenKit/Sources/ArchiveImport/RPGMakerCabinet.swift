import Foundation

nonisolated enum RPGMakerCabinet {
    private static let marker = Array("Software\\Enterbrain\\RPGVX".utf8)
    private static let maxStub = 8 << 20

    static func isPacked(_ url: URL) -> Bool {
        PEOverlay.read(url) { (pe: PEOverlay) -> Bool? in
            guard pe.start <= UInt64(maxStub), let header = pe.bytes(at: pe.start, count: 12), header.starts(with: Array("MSCF".utf8)),
                  UInt64(header.u32(8)) == pe.size - pe.start, let stub = pe.bytes(at: 0, count: Int(pe.start)) else { return false }
            return stub.firstRange(of: marker) != nil
        } ?? false
    }
}
