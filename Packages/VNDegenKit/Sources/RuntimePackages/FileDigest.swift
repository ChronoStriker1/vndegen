import CryptoKit

/// The digest an Inno Setup data entry stores for its file. Inno Setup 5 uses MD5 before 5.3.9 and SHA-1 from then on;
/// both serve only to detect damage here.
struct FileDigest {
    private enum State {
        case md5(Insecure.MD5), sha1(Insecure.SHA1)
    }

    private var state: State

    init(_ checksum: InnoHeaders.Checksum) {
        switch checksum {
        case .md5: state = .md5(Insecure.MD5())
        case .sha1: state = .sha1(Insecure.SHA1())
        }
    }

    mutating func update(_ bytes: [UInt8]) {
        switch state {
        case .md5(var hash):
            hash.update(data: bytes)
            state = .md5(hash)
        case .sha1(var hash):
            hash.update(data: bytes)
            state = .sha1(hash)
        }
    }

    func matches(_ checksum: InnoHeaders.Checksum) -> Bool {
        switch (state, checksum) {
        case (.md5(let hash), .md5(let expected)): Array(hash.finalize()) == expected
        case (.sha1(let hash), .sha1(let expected)): Array(hash.finalize()) == expected
        default: false
        }
    }
}
