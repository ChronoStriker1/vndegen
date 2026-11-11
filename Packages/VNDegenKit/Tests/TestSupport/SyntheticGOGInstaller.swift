import CLzmaDec
import CryptoKit
import Foundation

public enum SyntheticGOGInstaller {
    public enum Entry: Sendable {
        case file(String, [UInt8], check: String = "", languages: String = "")
        case galaxy(String, [UInt8], parts: Int, check: String = "")
    }

    public struct Built: Sendable {
        public let setup: Data
        public let slice: Data?
    }

    public static func info(name: String, gameId: String, rootGameId: String) -> Entry {
        let json = #"{"version": 1, "name": "\#(name)", "gameId": "\#(gameId)", "rootGameId": "\#(rootGameId)", "playTasks": []}"#
        return .file("{app}\\goggame-\(gameId).info", Array(json.utf8))
    }

    public static func make(appName: String, publisher: String = "GOG.com", languages: [String] = ["en-US"],
                            entries: [Entry], sliced: Bool = false, setupName: String = "setup") -> Built {
        var fileEntries: [(destination: String, location: Int, languages: String, check: String, before: String, after: String)] = []
        var payloads: [[UInt8]] = []
        for entry in entries {
            switch entry {
            case let .file(destination, contents, check, languages):
                fileEntries.append((destination, payloads.count, languages, check, "", ""))
                payloads.append(contents)
            case let .galaxy(path, contents, parts, check):
                let whole = Insecure.MD5.hash(data: contents).map { String(format: "%02x", $0) }.joined()
                let size = (contents.count + parts - 1) / max(parts, 1)
                for part in 0..<parts {
                    let piece = Array(contents[min(contents.count, part * size)..<min(contents.count, (part + 1) * size)])
                    let deflated = zlib(piece)
                    let name = Insecure.MD5.hash(data: deflated).map { String(format: "%02x", $0) }.joined()
                    let before = part == 0 ? "before_install('\(whole)', '\(path)', \(parts))" : ""
                    let after = "after_install('\(name)', \(deflated.count), \(piece.count))"
                    fileEntries.append(("{tmp}\\\(name.prefix(2))\\\(name.dropFirst(2).prefix(2))\\\(name)", payloads.count,
                                        "", check, before, after))
                    payloads.append(deflated)
                }
            }
        }

        var header: [UInt8] = []
        header += string(appName) + strings(3) + string(publisher) + string("https://www.gog.com") + strings(1)
        header += string("https://www.gog.com/support") + string("https://www.gog.com") + strings(3) + string(setupName)
        header += strings(7 + 4 + 1 + 1 + 1 + 1 + 2)
        header += strings(1 + 2) + strings(1)
        header += le32(UInt32(languages.count)) + [0, 0, 0, 0, 0].flatMap(le32)
        header += le32(0) + le32(UInt32(fileEntries.count)) + le32(UInt32(payloads.count)) + zeros(7 * 4)
        header += zeros(20 + 8) + zeros(1)
        header += zeros(20 + 8 + 8) + le32(1)
        header += zeros(5) + [4] + zeros(2) + zeros(2) + zeros(8)
        header += zeros(6)
        for language in languages {
            header += string(language) + strings(6) + strings(1) + strings(2) + zeros(4) + zeros(16) + zeros(1)
        }
        for file in fileEntries {
            header += strings(1) + string(file.destination) + strings(2) + strings(2)
            header += string(file.languages) + string(file.check) + string(file.after) + string(file.before)
            header += zeros(20) + le32(UInt32(file.location)) + zeros(4 + 8 + 2) + zeros(4) + [0]
        }

        let chunkStart: UInt64 = sliced ? 12 : 0
        var stream: [UInt8] = []
        var entriesBlock: [UInt8] = []
        for payload in payloads {
            entriesBlock += le32(0) + le32(0) + le32(UInt32(chunkStart)) + le64(UInt64(stream.count)) + le64(UInt64(payload.count))
            entriesBlock += le64(0)
            entriesBlock += Array(Insecure.SHA1.hash(data: payload)) + zeros(16) + [0x80, 0]
            stream += payload
        }
        let chunk = lzma2Stored(stream)
        let chunkSize = UInt64(chunk.count)
        for index in payloads.indices {
            let start = index * (4 + 4 + 4 + 8 + 8 + 8 + 20 + 16 + 2) + 4 + 4 + 4 + 8 + 8
            for (offset, byte) in le64(chunkSize).enumerated() { entriesBlock[start + offset] = byte }
        }

        let headerOffset = 0x40 + 40
        let version = Array("Inno Setup Setup Data (5.6.2) (u)".utf8)
        let blocks = block(header) + block(entriesBlock)
        let dataOffset = sliced ? 0 : headerOffset + 64 + blocks.count
        var table = Array("rDlPtS07".utf8) + [0x87] + Array("eVx".utf8) + zeros(16)
        table += le32(UInt32(headerOffset)) + le32(UInt32(dataOffset))
        table += le32(crc(table))
        var setup = zeros(0x30) + le32(0x6f6e_6e49) + le32(0x40) + le32(~UInt32(0x40)) + zeros(4)
        setup += table + version + zeros(64 - version.count) + blocks
        let data = Array("zlb".utf8) + [0x1a] + chunk
        if sliced {
            let slice = Array("idska32".utf8) + [0x1a] + le32(UInt32(12 + data.count)) + data
            return Built(setup: Data(setup), slice: Data(slice))
        }
        return Built(setup: Data(setup + data), slice: nil)
    }

    private static func lzma2Stored(_ bytes: [UInt8]) -> [UInt8] {
        var output: [UInt8] = [16]
        var first = true
        for start in stride(from: 0, to: bytes.count, by: 1 << 16) {
            let piece = bytes[start..<min(start + (1 << 16), bytes.count)]
            output += [first ? 1 : 2, UInt8((piece.count - 1) >> 8), UInt8((piece.count - 1) & 0xff)] + piece
            first = false
        }
        return output + [0]
    }

    public static func zlib(_ bytes: [UInt8]) -> [UInt8] {
        var size = compressBound(uLong(bytes.count))
        var output = [UInt8](repeating: 0, count: Int(size))
        _ = compress2(&output, &size, bytes, uLong(bytes.count), 6)
        return Array(output[0..<Int(size)])
    }

    private static func block(_ payload: [UInt8]) -> [UInt8] {
        var stored: [UInt8] = []
        for start in stride(from: 0, to: payload.count, by: 4096) {
            let piece = Array(payload[start..<min(start + 4096, payload.count)])
            stored += le32(crc(piece)) + piece
        }
        let info = le32(UInt32(stored.count)) + [0]
        return le32(crc(info)) + info + stored
    }

    private static func crc(_ bytes: [UInt8]) -> UInt32 { UInt32(crc32(0, bytes, uInt(bytes.count))) }
    private static func zeros(_ count: Int) -> [UInt8] { [UInt8](repeating: 0, count: count) }
    private static func le32(_ value: UInt32) -> [UInt8] { (0..<4).map { UInt8(truncatingIfNeeded: value >> ($0 * 8)) } }
    private static func le64(_ value: UInt64) -> [UInt8] { (0..<8).map { UInt8(truncatingIfNeeded: value >> ($0 * 8)) } }
    private static func string(_ text: String) -> [UInt8] {
        let utf16 = Array(text.utf16).flatMap { [UInt8($0 & 0xff), UInt8($0 >> 8)] }
        return le32(UInt32(utf16.count)) + utf16
    }
    private static func strings(_ count: Int) -> [UInt8] { zeros(4 * count) }
}
