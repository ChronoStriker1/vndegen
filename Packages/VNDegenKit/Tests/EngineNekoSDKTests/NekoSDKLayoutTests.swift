import Foundation
import Testing
import VNCore
@testable import EngineNekoSDK

@Suite struct NekoSDKLayoutTests {
    private func probe(_ entries: [String]) -> FolderProbe { FolderProbe(root: URL(fileURLWithPath: "/game"), entries: Set(entries)) }

    @Test func findsArchivesAtTheTop() {
        let layout = NekoSDKLayout(probe(["cfg.pak", "scr.pak", "game.exe"]))
        #expect(layout?.root == "")
        #expect(layout?.confidence == .certain)
    }

    @Test func findsArchivesOneFolderDownWhateverTheirCase() {
        let layout = NekoSDKLayout(probe(["Game", "Game/CFG.PAK", "Game/Scr.pak"]))
        #expect(layout?.root == "Game")
        #expect(layout?.evidence == "Game/CFG.PAK")
    }

    @Test func aTieGoesToTheFirstInSortedOrder() {
        let roots = (UInt8(ascii: "a")...UInt8(ascii: "z")).map { String(UnicodeScalar($0)) }
        let layout = NekoSDKLayout(probe(roots.flatMap { ["\($0)/cfg.pak", "\($0)/scr.pak"] }))
        #expect(layout?.root == "a")
    }

    @Test func theShallowestFolderWinsWhateverItsNameLength() {
        let layout = NekoSDKLayout(probe(["MainGameFolder/cfg.pak", "MainGameFolder/scr.pak", "x/y/cfg.pak", "x/y/scr.pak"]))
        #expect(layout?.root == "MainGameFolder")
    }

    @Test func wantsBothArchives() {
        #expect(NekoSDKLayout(probe(["cfg.pak", "data.pak"])) == nil)
        #expect(NekoSDKLayout(probe(["scr.pak"])) == nil)
    }
}
