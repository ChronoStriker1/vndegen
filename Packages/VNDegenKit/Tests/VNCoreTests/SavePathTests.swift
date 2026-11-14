import Foundation
import Testing
import VNCore

@Test func keepsARelativeSavePathInsideItsFolder() {
    #expect(SaveLocation.insideFolder("datasc.ksd") == "datasc.ksd")
    #expect(SaveLocation.insideFolder("Studio/savedata/datasc.ksd") == "Studio/savedata/datasc.ksd")
    #expect(SaveLocation.insideFolder("") == nil)
    #expect(SaveLocation.insideFolder("../library.json") == nil)
    #expect(SaveLocation.insideFolder("a/./b") == nil)
    #expect(SaveLocation.insideFolder("a//b") == nil)
    #expect(SaveLocation.insideFolder("a/") == nil)
}

@Test func mapsThePortableAndGameDataSavePaths() {
    #expect(SaveLocation.portableRelativePath("/VNDegen/Test Game/saves/1-1-LT1.save", folder: "saves") == "1-1-LT1.save")
    #expect(SaveLocation.portableRelativePath("/VNDegen/Test Game/saves/sync/persistent", folder: "saves") == "sync/persistent")
    #expect(SaveLocation.portableRelativePath("/Users/a/Library/GameData/Test Game/AppData/x.ksd", folder: "AppData") == "x.ksd")
    #expect(SaveLocation.portableRelativePath("/VNDegen/Test Game/1-1-LT1.save", folder: "saves") == nil)
    #expect(SaveLocation.portableRelativePath("/VNDegen/Test Game/Documents/x.ksd", folder: "AppData") == nil)
    #expect(SaveLocation.portableRelativePath("/home/deck/saves/x.save", folder: "saves") == nil)
}

@Test func splitsAProbePathIntoNameAndParent() {
    #expect(FolderProbe.name("game/data/startup.tjs") == "startup.tjs")
    #expect(FolderProbe.name("Game.exe") == "Game.exe")
    #expect(FolderProbe.parent("game/data/startup.tjs") == "game/data")
    #expect(FolderProbe.parent("Game.exe") == "")
}
