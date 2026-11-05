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

    @Test func wantsBothArchives() {
        #expect(NekoSDKLayout(probe(["cfg.pak", "data.pak"])) == nil)
        #expect(NekoSDKLayout(probe(["scr.pak"])) == nil)
    }
}
