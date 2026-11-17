import Foundation

public enum SyntheticEnigmaPack {
    public indirect enum Node {
        case file(String, [UInt8])
        case compressed(String, size: Int, stored: [UInt8])
        case folder(String, [Node])
    }

    public static func defaultFolder(_ children: [Node]) -> Node { .folder("%DEFAULT FOLDER%", children) }

    public static func make(_ roots: [Node], legacy: Bool = false, totalAdjustment: Int = 0) -> Data {
        var pack: [UInt8] = []
        var nextID: UInt32 = 1
        if legacy {
            pack += le32(15) + zeros(8) + le32(UInt32(roots.count)) + zeros(3)
            func emit(_ node: Node, parent: UInt32) {
                let id = nextID
                nextID += 1
                switch node {
                case let .folder(name, children):
                    let body = le32(id) + le32(parent) + le32(UInt32(children.count)) + utf16(name) + [3] + zeros(25) + zeros(4)
                    pack += le32(UInt32(body.count)) + body
                    children.forEach { emit($0, parent: id) }
                case let .file(name, bytes):
                    let body = le32(id) + le32(parent) + le32(0) + utf16(name) + [2] + legacyPayload(bytes.count, bytes.count)
                    pack += le32(UInt32(body.count)) + body + bytes
                case let .compressed(name, size, stored):
                    let body = le32(id) + le32(parent) + le32(0) + utf16(name) + [2] + legacyPayload(size, stored.count)
                    pack += le32(UInt32(body.count)) + body + stored
                }
            }
            roots.forEach { emit($0, parent: 0) }
        } else {
            var records: [UInt8] = []
            var data: [UInt8] = []
            func emit(_ node: Node, parent: UInt32) {
                let id = nextID
                nextID += 1
                switch node {
                case let .folder(name, children):
                    records += le32(0) + le32(id) + le32(parent) + le32(UInt32(children.count)) + utf16(name) + [3] + zeros(25)
                    children.forEach { emit($0, parent: id) }
                case let .file(name, bytes):
                    records += le32(0) + le32(id) + le32(parent) + le32(0) + utf16(name) + [2] + payload(bytes.count, bytes.count)
                    data += bytes
                case let .compressed(name, size, stored):
                    records += le32(0) + le32(id) + le32(parent) + le32(0) + utf16(name) + [2] + payload(size, stored.count)
                    data += stored
                }
            }
            roots.forEach { emit($0, parent: 0) }
            pack += le32(UInt32(15 + records.count - 4)) + zeros(8) + [UInt8(roots.count), 0, 0] + records + data
        }
        let total = UInt64(64 + pack.count + totalAdjustment)
        let header = Array("EVB\0".utf8) + le32(64) + zeros(8) + le32(1) + le32(legacy ? 1 : 3) + le32(64) + zeros(4)
            + le64(total - 64) + zeros(8) + le64(total) + zeros(8)

        var exe = zeros(0x200)
        exe[0] = 0x4d
        exe[1] = 0x5a
        exe.replaceSubrange(0x3c..<0x40, with: le32(0x40))
        exe.replaceSubrange(0x40..<0x44, with: Array("PE\0\0".utf8))
        exe.replaceSubrange(0x44..<0x46, with: [0x4c, 0x01])
        exe.replaceSubrange(0x46..<0x48, with: [1, 0])
        let section = 0x40 + 24
        exe.replaceSubrange(section..<(section + 8), with: Array(".enigma1".utf8))
        exe.replaceSubrange((section + 20)..<(section + 24), with: le32(0x200))
        return Data(exe + zeros(0x20) + header + pack + zeros(16))
    }

    private static func payload(_ size: Int, _ stored: Int) -> [UInt8] {
        zeros(2) + le32(UInt32(size)) + zeros(4) + zeros(24) + zeros(15) + le32(UInt32(stored))
    }

    private static func legacyPayload(_ size: Int, _ stored: Int) -> [UInt8] {
        zeros(2) + le32(UInt32(size)) + zeros(4) + zeros(24) + zeros(7) + le32(UInt32(stored)) + zeros(4)
    }

    private static func utf16(_ name: String) -> [UInt8] {
        name.utf16.flatMap { [UInt8($0 & 0xff), UInt8($0 >> 8)] } + [0, 0]
    }

    private static func zeros(_ count: Int) -> [UInt8] { [UInt8](repeating: 0, count: count) }
    private static func le32(_ value: UInt32) -> [UInt8] { (0..<4).map { UInt8(truncatingIfNeeded: value >> (8 * $0)) } }
    private static func le64(_ value: UInt64) -> [UInt8] { (0..<8).map { UInt8(truncatingIfNeeded: value >> (8 * $0)) } }
}
