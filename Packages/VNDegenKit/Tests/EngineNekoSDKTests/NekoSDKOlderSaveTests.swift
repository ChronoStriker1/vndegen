import CNekoSDK
import Foundation
import Testing
import TestSupport

// The older engine's saves (Test Games 83 and 87, core/save.cpp): the choice's picture 236 bytes after the flow, sprite
// records of 383 bytes with x and y as ints, and fewer sound channels. VNDegen's save of a scene with Test Game 83's
// layout, in a folder of its own with no menu table, holds its standing picture where the PC's saves do, and loads back.
@Suite struct NekoSDKOlderSaveTests {
    // A 24-bit BMP of one colour, `w` by `h`, as blue, green, red.
    private func bmp(_ w: Int, _ h: Int, _ bgr: [UInt8]) -> Data {
        let row = (w * 3 + 3) & ~3
        var d = Data("BM".utf8)
        func put(_ v: UInt32) { d.append(contentsOf: withUnsafeBytes(of: v.littleEndian, Array.init)) }
        put(UInt32(54 + row * h)); put(0); put(54)
        put(40); put(UInt32(w)); put(UInt32(h))
        d.append(contentsOf: [1, 0, 24, 0])
        put(0); put(UInt32(row * h)); put(0); put(0); put(0); put(0)
        var line = Data()
        for _ in 0..<w { line.append(contentsOf: bgr) }
        line.append(Data(repeating: 0, count: row - w * 3))
        for _ in 0..<h { d.append(line) }
        return d
    }

    private func pixel(_ e: OpaquePointer, _ x: Int, _ y: Int) -> [UInt8] {
        _ = vnneko_step(e, 16)
        var w: Int32 = 0, h: Int32 = 0, changed = false
        guard let px = vnneko_frame(e, &w, &h, &changed) else { return [] }
        let at = (y * Int(w) + x) * 4
        return [px[at], px[at + 1], px[at + 2]]
    }

    private func int32(_ data: Data, _ at: Int) -> Int32 {
        data.subdata(in: (data.startIndex + at)..<(data.startIndex + at + 4)).withUnsafeBytes { $0.loadUnaligned(as: Int32.self) }
    }

    private func game(_ folder: String = "layoutold") throws -> URL {
        try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\tvndegen\\\(folder)\nFirstFlow\tflow\\t.txt\n".utf8),
            "bg.pak": NekoSDKFlowFixture.archive([("g.bmp", bmp(4, 4, [0, 255, 0]))]),
            "sc.pak": NekoSDKFlowFixture.archive([("r.bmp", bmp(101, 81, [0, 0, 255]))]),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 10, next: 3, s: [1: "bg\\g.bmp"]),
                // Place 3: (1280 - 101) / 2 - 400 + 5 = 194, and (720 - 81) / 2 + 7 = 326.
                .init(op: 13, next: 4, p: [0: 3, 2: 5, 3: 7], s: [1: "sc\\r.bmp"]),
                .init(op: 9, next: 5),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
        ])
    }

    private func saves(_ data: URL, _ folder: String = "layoutold") -> URL {
        data.appendingPathComponent("AppData/Roaming/vndegen/\(folder)/save")
    }

    private func file(_ data: URL, _ name: String, _ folder: String = "layoutold") throws -> URL {
        try #require(try FileManager.default.contentsOfDirectory(at: saves(data, folder), includingPropertiesForKeys: nil)
            .first { $0.lastPathComponent.lowercased() == name })
    }

    @Test func aSaveInTheOlderLayoutKeepsItsPictureAndLoads() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<10 { _ = vnneko_step(first, 16) }
        #expect(pixel(first, 194, 326) == [255, 0, 0])
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)

        // The standing picture's record: slot 600 for place 3, and x and y as ints 0xD5 after its two paths.
        let save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: try file(data, "save001.sav")))
        let path = try #require(save.range(of: Data("sc\\r.bmp".utf8)))
        let at = path.lowerBound - save.startIndex
        #expect(int32(save, at - 12) == 600)
        // After the name come the second name, empty in the older engine's saves, then the fixed part.
        let fixed = at + 8 + 4
        #expect(int32(save, at + 8) == 0)
        #expect(int32(save, fixed + 0xD5) == 194)
        #expect(int32(save, fixed + 0xD9) == 326)
        // The flow's name at 0xCD, here the patch folder's, and the choice's picture, here none, 236 bytes after it.
        let flow = Int(int32(save, 0xCD))
        #expect(flow == "sys_1\\flow\\t.txt".utf8.count)
        #expect(int32(save, 0xCD + 4 + flow + 236) == 0)

        // A new run, and the load brings back the save's picture and background.
        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(second) }
        #expect(vnneko_debug_load(second, 1))
        for _ in 0..<3 { _ = vnneko_step(second, 16) }
        #expect(pixel(second, 194, 326) == [255, 0, 0])
        #expect(pixel(second, 0, 0) == [0, 255, 0])
    }

    // A PC save of a standing picture with a named animation running names command 19's text 0x125 into the record's
    // fixed part, and all after it moves by the text's length (Test Game 87, set 1). Such a save loads.
    @Test func aRecordWithAnAnimationsTextLoads() throws {
        let root = try game()
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<10 { _ = vnneko_step(first, 16) }
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)

        var save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: try file(data, "save001.sav")))
        let path = try #require(save.range(of: Data("sc\\r.bmp".utf8)))
        let text = Data("relative_pos_y,0,0,30,liner".utf8)
        let at = path.lowerBound - save.startIndex + 8 + 4 + 0x125
        #expect(int32(save, at) == 0)
        save.replaceSubrange((save.startIndex + at)..<(save.startIndex + at + 4), with: withUnsafeBytes(of: Int32(text.count).littleEndian, { Data($0) }))
        save.insert(contentsOf: text, at: save.startIndex + at + 4)
        try NekoSDKFlowFixture.packed(save).write(to: saves(data).appendingPathComponent("save002.sav"))

        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(second) }
        #expect(vnneko_debug_load(second, 2))
        for _ in 0..<3 { _ = vnneko_step(second, 16) }
        #expect(pixel(second, 194, 326) == [255, 0, 0])
        #expect(pixel(second, 0, 0) == [0, 255, 0])
    }

    // Test Game 96's layout: the older engine's records, 396 bytes after their names with 26 zeros before each, and the
    // newer place for the choice's picture with a second picture's name after it, the two every PC save of it names.
    @Test func aSaveInTestGame96sLayoutLoads() throws {
        let root = try game("layout96")
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<10 { _ = vnneko_step(first, 16) }
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)

        let save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: try file(data, "save001.sav", "layout96")))
        let flow = Int(int32(save, 0xCD))
        let choice = 0xCD + 4 + flow + 4196
        let names = ["obj\\a003_001.bmp", "obj\\t000_000.bmp"]
        #expect(Int(int32(save, choice)) == names[0].utf8.count)
        #expect(save.subdata(in: (save.startIndex + choice + 4)..<(save.startIndex + choice + 20)) == Data(names[0].utf8))
        #expect(Int(int32(save, choice + 20)) == names[1].utf8.count)
        // The date block, 39,592 bytes after the second name: 1 at 240.
        let date = choice + 40 + 39592
        #expect(save[save.startIndex + date + 240] == 1)
        // The standing picture's record, the second, 26 zeros after the first's 396 bytes past its empty names.
        let path = try #require(save.range(of: Data("sc\\r.bmp".utf8)))
        let at = path.lowerBound - save.startIndex
        #expect(int32(save, at - 12) == 600)
        #expect(int32(save, at + 8 + 4 + 0xD5) == 194)

        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(second) }
        #expect(vnneko_debug_load(second, 1))
        for _ in 0..<3 { _ = vnneko_step(second, 16) }
        #expect(pixel(second, 194, 326) == [255, 0, 0])
        #expect(pixel(second, 0, 0) == [0, 255, 0])

        // 96's PC saves name their scene with a doubled backslash, as its chapter list writes it: the scene still loads.
        var doubled = save
        let end = try #require(doubled.range(of: Data("s.txt".utf8), options: .backwards)).upperBound - doubled.startIndex
        var start = end - 5
        while start > 4 && Int(int32(doubled, start - 4)) != end - start { start -= 1 }
        let name = String(decoding: doubled.subdata(in: (doubled.startIndex + start)..<(doubled.startIndex + end)), as: UTF8.self)
        let twice = Data(name.replacingOccurrences(of: "\\", with: "\\\\").utf8)
        doubled.replaceSubrange((doubled.startIndex + start - 4)..<(doubled.startIndex + end), with: withUnsafeBytes(of: Int32(twice.count).littleEndian, { Data($0) }) + twice)
        try NekoSDKFlowFixture.packed(doubled).write(to: saves(data, "layout96").appendingPathComponent("save002.sav"))
        #expect(vnneko_debug_load(second, 2))
        for _ in 0..<3 { _ = vnneko_step(second, 16) }
        #expect(String(cString: vnneko_debug_status(second)).contains("s.txt"), "\(String(cString: vnneko_debug_status(second)))")
        #expect(pixel(second, 194, 326) == [255, 0, 0])
    }

    // A PC save of Test Game 96 from before a new game's first question names neither picture, and all after them sits
    // 32 bytes earlier: read and built again it keeps them empty, where a save of the runtime's own names both.
    @Test func aSaveWithNoPictureNamesIsBuiltAgainWithNone() throws {
        let root = try game("layout96")
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        #expect(vnneko_debug_save(e, 1))
        vnneko_stop(e)
        var save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: try file(data, "save001.sav", "layout96")))
        let flow = Int(int32(save, 0xCD))
        let choice = 0xCD + 4 + flow + 4196
        #expect(int32(save, choice) == 16 && int32(save, choice + 20) == 16)
        // Both names taken out, their lengths left as zeros.
        save.replaceSubrange((save.startIndex + choice)..<(save.startIndex + choice + 40), with: Data(count: 8))
        var built = [UInt8](repeating: 0, count: save.count + 64)
        let size = save.withUnsafeBytes { vnneko_debug_rebuild_save("vndegen\\layout96", $0.bindMemory(to: UInt8.self).baseAddress, save.count, &built, built.count) }
        #expect(size == save.count)
        #expect(Data(built.prefix(size)) == save)
    }

    // Test Game 90's layout: the newer engine's sprite records with x and y as floats 0xD5 after their names, a shorter
    // last block, and a tail without the GOOD Timing fields.
    @Test func aSaveInTestGame90sLayoutLoads() throws {
        let root = try game("layout90")
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<10 { _ = vnneko_step(first, 16) }
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)

        let save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: try file(data, "save001.sav", "layout90")))
        let path = try #require(save.range(of: Data("sc\\r.bmp".utf8)))
        let at = path.lowerBound - save.startIndex
        #expect(int32(save, at - 12) == 600)
        let x = save.subdata(in: (save.startIndex + at + 12 + 0xD5)..<(save.startIndex + at + 12 + 0xD9)).withUnsafeBytes { $0.loadUnaligned(as: Float.self) }
        #expect(x == 194)
        // A shown sprite holds 255 as its alpha, as 90's PC writes it and needs to draw it (set 1), and the head's 1.0s
        // where 90's PC has them, 105 into the record past its names.
        #expect(int32(save, at + 12 + 249) == 255)
        for place in [105, 125, 145, 165] { #expect(int32(save, at + 12 + place) == 0x3F80_0000, "\(place)") }
        #expect(int32(save, at + 12 + 113) == 0)

        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(second) }
        #expect(vnneko_debug_load(second, 1))
        for _ in 0..<3 { _ = vnneko_step(second, 16) }
        #expect(pixel(second, 194, 326) == [255, 0, 0])
        #expect(pixel(second, 0, 0) == [0, 255, 0])
    }

    // A save read and built again keeps a sprite's alpha as it is, a value above 256 too, as a PC save of Test Game 94
    // can hold: only Test Game 90's layout changes one, and only the 255 its shown sprites hold.
    @Test func aRebuiltSaveKeepsAnAlphaAbove256() throws {
        // The alpha's place past the record's names: 257 in Test Game 94's layout, 249 in 90's.
        for (folder, place) in [("layout94", 257), ("layout90", 249)] {
            let root = try game(folder)
            defer { try? FileManager.default.removeItem(at: root) }
            let data = root.appendingPathComponent("data")
            let e = try #require(NekoSDKFlowFixture.start(root, data: data))
            for _ in 0..<10 { _ = vnneko_step(e, 16) }
            #expect(vnneko_debug_save(e, 1))
            vnneko_stop(e)
            var save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: try file(data, "save001.sav", folder)))
            let path = try #require(save.range(of: Data("sc\\r.bmp".utf8)))
            let at = path.lowerBound - save.startIndex + 12 + place
            #expect(int32(save, at) == (folder == "layout90" ? 255 : 256), "\(folder)")
            for alpha in [Int32(512), 300, 255, 256, 100] {
                save.replaceSubrange((save.startIndex + at)..<(save.startIndex + at + 4), with: withUnsafeBytes(of: alpha.littleEndian, { Data($0) }))
                var built = [UInt8](repeating: 0, count: save.count + 64)
                let size = save.withUnsafeBytes { vnneko_debug_rebuild_save("vndegen\\\(folder)", $0.bindMemory(to: UInt8.self).baseAddress, save.count, &built, built.count) }
                #expect(size == save.count, "\(folder) \(alpha)")
                #expect(Data(built.prefix(size)) == save, "\(folder) \(alpha)")
            }
        }
    }

    @Test func aRebuiltSaveKeepsAPicturelessRecordAsItWas() throws {
        let root = try game("layout90")
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        #expect(vnneko_debug_save(e, 1))
        vnneko_stop(e)
        var save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: try file(data, "save001.sav", "layout90")))
        let name = try #require(save.range(of: Data("sc\\r.bmp".utf8)))
        let length = name.lowerBound - 4
        #expect(int32(save, length - save.startIndex) == 8)
        #expect(int32(save, length - save.startIndex - 4) == 0)
        save.replaceSubrange(length..<name.upperBound, with: Data(count: 4))
        let alpha = length - save.startIndex + 4 + 4 + 249
        #expect(int32(save, alpha) == 255)
        save.replaceSubrange((save.startIndex + alpha)..<(save.startIndex + alpha + 4), with: withUnsafeBytes(of: Int32(256).littleEndian, { Data($0) }))
        var built = [UInt8](repeating: 0, count: save.count + 64)
        let size = save.withUnsafeBytes { vnneko_debug_rebuild_save("vndegen\\layout90", $0.bindMemory(to: UInt8.self).baseAddress, save.count, &built, built.count) }
        #expect(size == save.count)
        #expect(Data(built.prefix(size)) == save)
    }

    @Test func aFlipbookOnItsFifthPictureIsNotNoise() throws {
        let root = try game("layout94")
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<10 { _ = vnneko_step(first, 16) }
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)
        let url = try file(data, "save001.sav", "layout94")
        var save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: url))
        let name = try #require(save.range(of: Data("sc\\r.bmp".utf8))).lowerBound
        #expect(save[name - 13] == 1)
        save[name - 13] = 0
        save.replaceSubrange((name - 8)..<(name - 4), with: withUnsafeBytes(of: Int32(4).littleEndian, { Data($0) }))
        try NekoSDKFlowFixture.packed(save).write(to: url)
        try? FileManager.default.removeItem(at: try file(data, "save001.vnd", "layout94"))
        let second = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(second) }
        #expect(vnneko_debug_load(second, 1))
        for _ in 0..<3 { _ = vnneko_step(second, 16) }
        #expect(pixel(second, 194, 326) == [255, 0, 0])
    }

    @Test func aSaveWithASecondListOfRecordsReadsAndIsBuiltAgain() throws {
        let root = try game("layout90")
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let first = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<10 { _ = vnneko_step(first, 16) }
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)
        let url = try file(data, "save001.sav", "layout90")
        var save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: url))
        let name = try #require(save.range(of: Data("sc\\r.bmp".utf8))).lowerBound
        let record = (name - 14)..<(name - 14 + 452 + 8)
        let block = record.upperBound - save.startIndex
        #expect(int32(save, block + 2) == -1)
        #expect(int32(save, block + 22) == 0)
        #expect(int32(save, block + 26) == 0)
        var second = save.subdata(in: record)
        second.replaceSubrange((second.startIndex + 2)..<(second.startIndex + 6), with: withUnsafeBytes(of: Int32(2000).littleEndian, { Data($0) }))
        save.replaceSubrange((save.startIndex + block + 26)..<(save.startIndex + block + 30), with: withUnsafeBytes(of: Int32(1).littleEndian, { Data($0) }))
        save.insert(contentsOf: second, at: save.startIndex + block + 70)
        var built = [UInt8](repeating: 0, count: save.count + 64)
        let size = save.withUnsafeBytes { vnneko_debug_rebuild_save("vndegen\\layout90", $0.bindMemory(to: UInt8.self).baseAddress, save.count, &built, built.count) }
        #expect(size == save.count)
        #expect(Data(built.prefix(size)) == save)
        try NekoSDKFlowFixture.packed(save).write(to: url)
        try? FileManager.default.removeItem(at: try file(data, "save001.vnd", "layout90"))
        let again = try #require(NekoSDKFlowFixture.start(root, data: data))
        defer { vnneko_stop(again) }
        #expect(vnneko_debug_load(again, 1))
        for _ in 0..<3 { _ = vnneko_step(again, 16) }
        #expect(pixel(again, 194, 326) == [255, 0, 0])
    }

    @Test func aChanceCountWithNoSecondListStillReadsAsBefore() throws {
        let root = try game("layout90")
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        #expect(vnneko_debug_save(e, 1))
        vnneko_stop(e)
        let url = try file(data, "save001.sav", "layout90")
        let made = try NekoSDKFlowFixture.unpacked(Data(contentsOf: url))
        let name = try #require(made.range(of: Data("sc\\r.bmp".utf8))).lowerBound
        let block = name - 14 + 452 + 8 - made.startIndex
        #expect(int32(made, block + 26) == 0)
        #expect(made[made.startIndex + block + 70] == 0 && made[made.startIndex + block + 71] == 0)
        try? FileManager.default.removeItem(at: try file(data, "save001.vnd", "layout90"))
        for (count, lead, kind) in [(Int32(1), UInt8(0), UInt8(0)), (2, 0, 0), (64, 0, 0), (1, 1, 0), (1, 1, 1), (1, 1, 2), (65, 1, 0)] {
            var save = made
            save.replaceSubrange((save.startIndex + block + 26)..<(save.startIndex + block + 30), with: withUnsafeBytes(of: count.littleEndian, { Data($0) }))
            save[save.startIndex + block + 70] = lead
            save[save.startIndex + block + 71] = kind
            var built = [UInt8](repeating: 0, count: save.count + 64)
            let size = save.withUnsafeBytes { vnneko_debug_rebuild_save("vndegen\\layout90", $0.bindMemory(to: UInt8.self).baseAddress, save.count, &built, built.count) }
            #expect(size == save.count, "\(count) \(lead) \(kind)")
            #expect(Data(built.prefix(size)) == save, "\(count) \(lead) \(kind)")
            try NekoSDKFlowFixture.packed(save).write(to: url)
            let again = try #require(NekoSDKFlowFixture.start(root, data: data))
            #expect(vnneko_debug_load(again, 1), "\(count) \(lead) \(kind)")
            for _ in 0..<3 { _ = vnneko_step(again, 16) }
            #expect(String(cString: vnneko_debug_status(again)).contains("s.txt"), "\(count) \(lead) \(kind)")
            #expect(pixel(again, 194, 326) == [255, 0, 0], "\(count) \(lead) \(kind)")
            vnneko_stop(again)
        }
    }

    @Test func aSecondListOfTwoRecordsReadsAndABrokenSecondOneIsNoList() throws {
        let root = try game("layout90")
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let e = try #require(NekoSDKFlowFixture.start(root, data: data))
        for _ in 0..<10 { _ = vnneko_step(e, 16) }
        #expect(vnneko_debug_save(e, 1))
        vnneko_stop(e)
        var save = try NekoSDKFlowFixture.unpacked(Data(contentsOf: try file(data, "save001.sav", "layout90")))
        let name = try #require(save.range(of: Data("sc\\r.bmp".utf8))).lowerBound
        let record = (name - 14)..<(name - 14 + 452 + 8)
        let block = record.upperBound - save.startIndex
        var second = save.subdata(in: record)
        second.replaceSubrange((second.startIndex + 2)..<(second.startIndex + 6), with: withUnsafeBytes(of: Int32(2000).littleEndian, { Data($0) }))
        var third = second
        third.replaceSubrange((third.startIndex + 2)..<(third.startIndex + 6), with: withUnsafeBytes(of: Int32(2001).littleEndian, { Data($0) }))
        save.replaceSubrange((save.startIndex + block + 26)..<(save.startIndex + block + 30), with: withUnsafeBytes(of: Int32(2).littleEndian, { Data($0) }))
        save.insert(contentsOf: second + third, at: save.startIndex + block + 70)
        func rebuilt(_ plain: Data) -> Data {
            var built = [UInt8](repeating: 0, count: plain.count + 64)
            let size = plain.withUnsafeBytes { vnneko_debug_rebuild_save("vndegen\\layout90", $0.bindMemory(to: UInt8.self).baseAddress, plain.count, &built, built.count) }
            return Data(built.prefix(size))
        }
        #expect(rebuilt(save) == save)
        var broken = save
        broken[broken.startIndex + block + 70 + second.count] = 0
        #expect(rebuilt(broken).isEmpty)
    }
}
