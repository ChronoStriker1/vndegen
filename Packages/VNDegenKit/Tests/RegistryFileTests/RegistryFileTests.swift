import Foundation
import Testing
@testable import RegistryFile

/// The fixtures came from regashii 356cf0d and Ludusavi 95b651a's own `Hives` conversion, fed `ludusavi-registry.yaml`:
/// `ludusavi-registry.reg` is what Ludusavi writes as `registry.reg`, and its SHA-1 is the hash Ludusavi records,
/// 2fa7b963f765cfa2528c06f2dd94bdf99ee75323.
@Suite struct RegistryFileTests {
    private static let fixtures = URL(filePath: #filePath).deletingLastPathComponent().appending(path: "Fixtures")
    private static func fixture(_ name: String) throws -> String {
        try String(contentsOf: fixtures.appending(path: name), encoding: .utf8)
    }

    private static let game = #"HKEY_CURRENT_USER\Software\Studio\Game"#

    /// The fixture's registry, built by hand from `ludusavi-registry.yaml`.
    private static var expected: RegistryData {
        var data = RegistryData()
        data.setValues([
            "": .sz("default"),
            "LongSave_h99": .binary(Array(0...60)),
            "PlayerName_h1": .binary([227, 130, 162, 227, 130, 164, 0]),
            "Screenmanager Resolution Width_h182942802": .dword(1280),
            #"quote"and\slash"#: .sz(#"a "quoted" C:\path"#),
            "unity.player_session_count_h922449978": .binary([49, 51, 0]),
            "volume_h3343231": .qword(4_607_182_418_800_017_408),
        ], of: game)
        data.setValues(["inner": .dword(7)], of: game + #"\Sub"#)
        data.setValues([
            "expand": .expandSz(#"%USERPROFILE%\x"#),
            "multi": .multiSz("one\ntwo"),
            "newline": .sz("line1\nline2"),
            "rawdword": .raw(kind: 4, bytes: [1, 0, 0, 0]),
            "rawnone": .raw(kind: 0, bytes: [9, 8]),
        ], of: #"HKEY_CURRENT_USER\Software\Studio\Other"#)
        data.setValues(["名前": .sz("値")], of: #"HKEY_CURRENT_USER\Software\スタジオ\ゲーム"#)
        data.setValues(["Installed": .dword(1)], of: #"HKEY_LOCAL_MACHINE\Software\Studio"#)
        return data
    }

    @Test func writesWhatLudusaviWrites() throws {
        #expect(Self.expected.regText == (try Self.fixture("ludusavi-registry.reg")))
    }

    @Test func readsWhatLudusaviWrites() throws {
        #expect(try RegistryData(reg: Self.fixture("ludusavi-registry.reg")) == Self.expected)
    }

    /// A file VNDegen rewrites after changing one key must keep every other key as Ludusavi wrote it.
    @Test func rewritingLudusavisFileChangesNothingElse() throws {
        let text = try Self.fixture("ludusavi-registry.reg")
        var data = try RegistryData(reg: text)
        #expect(data.regText == text)
        data.setValues(["Screenmanager Resolution Width_h182942802": .dword(1920)], of: Self.game.uppercased())
        let changed = data.regText.components(separatedBy: "\n\n")
        let original = text.components(separatedBy: "\n\n")
        #expect(changed.count == original.count)
        #expect(changed[1] == "[HKEY_CURRENT_USER\\Software\\Studio\\Game]\n\"Screenmanager Resolution Width_h182942802\"=dword:00000780")
        #expect(Array(changed.dropFirst(2)) == Array(original.dropFirst(2)))
    }

    @Test func matchesKeysWithoutCase() throws {
        let data = try RegistryData(reg: Self.fixture("ludusavi-registry.reg"))
        #expect(data.values(of: #"hkey_current_user\software\studio\game\"#)?["PlayerName_h1"] == .binary([227, 130, 162, 227, 130, 164, 0]))
        #expect(data.only(Self.game.lowercased()).keys.keys.sorted() == [Self.game])
        #expect(data.values(of: Self.game + #"\Missing"#) == nil)
    }

    /// What Wine's `regedit /E` writes for a key: UTF-16 with a byte order mark, CRLF, the default value as `@`, a
    /// subkey after it, and Wine's own escapes in strings.
    @Test func readsWinesExport() throws {
        let export = """
            Windows Registry Editor Version 5.00\r
            \r
            [HKEY_CURRENT_USER\\Software\\Studio\\Game]\r
            @="def"\r
            "Screenmanager Is Fullscreen mode_h3981298716"=dword:00000003\r
            "unity.cloud_userid_h2665564582"=hex:61,62,63,64,65,66,30,31,32,33,34,35,36,37,38,39,\\\r
              61,62,63,00\r
            "line"="a\\nb"\r
            \r
            [HKEY_CURRENT_USER\\Software\\Studio\\Game\\Sub]\r
            "x"=dword:00000001\r

            """
        let data = try RegistryData(reg: RegistryData.utf16File(export))
        #expect(data.values(of: Self.game) == [
            "": .sz("def"),
            "Screenmanager Is Fullscreen mode_h3981298716": .dword(3),
            "unity.cloud_userid_h2665564582": .binary(Array("abcdef0123456789abc".utf8) + [0]),
            "line": .sz("a\nb"),
        ])
        #expect(data.values(of: Self.game + #"\Sub"#) == ["x": .dword(1)])
    }

    @Test func dropsDeletedKeysAndValues() throws {
        let text = """
            Windows Registry Editor Version 5.00

            [HKEY_CURRENT_USER\\Software\\A]
            "gone"=dword:00000001
            "kept"=dword:00000002
            "gone"=-

            [-HKEY_CURRENT_USER\\Software\\B]

            [HKEY_CURRENT_USER\\Software\\C]
            "c"=dword:00000003

            [-HKEY_CURRENT_USER\\Software\\C]

            [HKEY_CURRENT_USER\\Software\\C]
            "again"=dword:00000004

            """
        let data = try RegistryData(reg: text)
        #expect(data.keys == [
            #"HKEY_CURRENT_USER\Software\A"#: ["kept": .dword(2)],
            #"HKEY_CURRENT_USER\Software\C"#: ["again": .dword(4)],
        ])
    }

    @Test func refusesWhatIsNotARegFile() {
        #expect(throws: RegistryData.ReadError.header) { try RegistryData(reg: "[HKEY_CURRENT_USER\\Software]\n") }
        #expect(throws: RegistryData.ReadError.encoding) { try RegistryData(reg: Data([0xC3, 0x28])) }
    }

    @Test func readsREGEDIT4() throws {
        let data = try RegistryData(reg: "REGEDIT4\n\n[HKEY_CURRENT_USER\\Software\\A]\n\"s\"=hex(2):25,41,25,00\n")
        #expect(data.values(of: #"HKEY_CURRENT_USER\Software\A"#) == ["s": .raw(kind: 2, bytes: [0x25, 0x41, 0x25, 0])])
    }

    /// A name with a line break, which Wine's `\n` escape can produce, must not open a section of its own.
    @Test func leavesOutNamesThatWouldBreakALine() throws {
        let text = """
            Windows Registry Editor Version 5.00

            [HKEY_CURRENT_USER\\Software\\S\\G]
            "a\\n\\n[HKEY_CURRENT_USER]\\nb"=dword:00000001
            "z"=dword:00000002

            """
        let data = try RegistryData(reg: text)
        #expect(data.keys.count == 1)
        let written = data.regText
        #expect(written == "Windows Registry Editor Version 5.00\n\n[HKEY_CURRENT_USER\\Software\\S\\G]\n\"z\"=dword:00000002\n")
        #expect(try RegistryData(reg: written).keys.keys.sorted() == [#"HKEY_CURRENT_USER\Software\S\G"#])
    }

    @Test func writesUTF16WithAByteOrderMark() {
        #expect(RegistryData.utf16File("Aあ") == Data([0xFF, 0xFE, 0x41, 0x00, 0x42, 0x30]))
    }
}
