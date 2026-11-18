import Foundation

/// The data track of a CD image's cue sheet: the BIN file it names and how its sectors hold the 2048-byte data.
nonisolated struct CueSheet: Equatable, Sendable {
    var binName: String
    var sectorSize: Int32
    var dataOffset: Int32

    init?(_ text: String) {
        var file: String?
        for raw in text.split(whereSeparator: \.isNewline) {
            let line = raw.trimmingCharacters(in: .whitespaces)
            if let match = line.wholeMatch(of: /FILE\s+"([^"]+)"\s+\S+/.ignoresCase()) ?? line.wholeMatch(of: /FILE\s+(\S+)\s+\S+/.ignoresCase()) {
                file = String(match.1)
            } else if let match = line.wholeMatch(of: /TRACK\s+\d+\s+(MODE1|MODE2)\/(2352|2048|2336)/.ignoresCase()), let file {
                let mode = match.1.uppercased(), size = Int32(match.2)!
                switch (mode, size) {
                case ("MODE1", 2352): self.init(binName: file, sectorSize: 2352, dataOffset: 16)
                case ("MODE1", 2048): self.init(binName: file, sectorSize: 2048, dataOffset: 0)
                case ("MODE2", 2352): self.init(binName: file, sectorSize: 2352, dataOffset: 24)
                case ("MODE2", 2336): self.init(binName: file, sectorSize: 2336, dataOffset: 8)
                default: return nil
                }
                return
            }
        }
        return nil
    }

    init(binName: String, sectorSize: Int32, dataOffset: Int32) {
        self.binName = binName.replacing("\\", with: "/").split(separator: "/").last.map(String.init) ?? binName
        self.sectorSize = sectorSize
        self.dataOffset = dataOffset
    }

    static func read(_ url: URL) -> CueSheet? {
        guard let data = try? Data(contentsOf: url), data.count < 1 << 20 else { return nil }
        let text = String(data: data, encoding: .utf8) ?? String(data: data, encoding: .windowsJapanese) ?? String(decoding: data, as: UTF8.self)
        return CueSheet(text)
    }
}
