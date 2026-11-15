import CNekoSDK
import Foundation
import Testing
import TestSupport

nonisolated(unsafe) private let movieFrame: UnsafeMutablePointer<UInt8> = {
    let pixels = UnsafeMutablePointer<UInt8>.allocate(capacity: 16)
    pixels.initialize(repeating: 255, count: 16)
    return pixels
}()

nonisolated(unsafe) private var stillMovies = VNNekoMovies(
    open: { _, _, _, _, info, _, _ in
        info?.pointee = VNNekoMovieInfo(width: 2, height: 2, fps: 30, duration: 10, audioRate: 0, audioChannels: 0)
        return UnsafeMutableRawPointer(movieFrame)
    },
    frame: { _, _, changed, ended in
        changed?.pointee = 1
        ended?.pointee = 0
        return UnsafePointer(movieFrame)
    },
    audio: { _, _, _ in 0 },
    seek: { _, _ in },
    close: { _ in })

@Suite struct NekoSDKCameraMovieTests {
    private func start(_ root: URL, data: URL) -> OpaquePointer? {
        let gamePath: String = root.path, dataDirPath: String = data.path
        return gamePath.withCString { rootPath in
            dataDirPath.withCString { dataPath in
                withUnsafePointer(to: &stillMovies) { movies in
                    var launch = VNNekoLaunch(gameRoot: rootPath, dataDir: dataPath, logPath: nil, fontDir: nil, movies: movies)
                    return vnneko_start(&launch)
                }
            }
        }
    }

    private func int32(_ data: Data, _ at: Int) -> Int32 {
        data.subdata(in: (data.startIndex + at)..<(data.startIndex + at + 4)).withUnsafeBytes { $0.loadUnaligned(as: Int32.self) }
    }

    private func movieKind(_ save: Data, _ name: String) -> Int32? {
        guard let at = save.range(of: Data(name.utf8)) else { return nil }
        return int32(save, at.lowerBound - save.startIndex - 8)
    }

    @Test func manualControlPicksTheHardMovieAndOtherwiseTheCameraOne() throws {
        let root = try NekoSDKFlowFixture.game([
            "cfg/init.ini": Data("UserDataDirectory\twaffle\\animother\nFirstFlow\tflow\\t.txt\n".utf8),
            "flow/t.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 1, next: 3, s: [1: "scr\\s.txt"]),
                .init(op: 2, next: 0),
            ]),
            "scr/s.txt": NekoSDKFlowFixture.flow([
                .init(op: 0, next: 2),
                .init(op: 100, next: 3, p: [3: 100, 9: 1], s: [1: "movie\\mv_hard"]),
                .init(op: 9, next: 4),
                .init(op: 2, next: 0, p: [0: 100_000]),
            ]),
            "movie/mv_hard": Data(count: 64),
            "movie/mv_camera": Data(count: 64),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appendingPathComponent("data")
        let saves = data.appendingPathComponent("AppData/Roaming/waffle/animother/save")

        let first = try #require(start(root, data: data))
        #expect(vnneko_debug_jump(first, "flow\\t.txt", 0))
        for _ in 0..<10 { _ = vnneko_step(first, 16) }
        #expect(vnneko_debug_save(first, 1))
        vnneko_stop(first)
        let camera = try NekoSDKFlowFixture.unpacked(Data(contentsOf: saves.appendingPathComponent("save001.sav")))
        #expect(movieKind(camera, "movie\\mv_camera") == 13)
        #expect(camera.range(of: Data("movie\\mv_hard".utf8)) == nil)

        var rebuilt = [UInt8](repeating: 0, count: camera.count + 4096)
        let size = camera.withUnsafeBytes { plain in
            vnneko_debug_rebuild_save("waffle\\animother", plain.bindMemory(to: UInt8.self).baseAddress, camera.count, &rebuilt, rebuilt.count)
        }
        #expect(Data(rebuilt.prefix(size)) == camera)

        try Data("anim_manual 1\n".utf8).write(to: saves.appendingPathComponent("vndegen_settings.txt"))
        let second = try #require(start(root, data: data))
        defer { vnneko_stop(second) }
        #expect(vnneko_debug_load(second, 1))
        for _ in 0..<3 { _ = vnneko_step(second, 16) }
        #expect(vnneko_debug_save(second, 2))
        let hard = try NekoSDKFlowFixture.unpacked(Data(contentsOf: saves.appendingPathComponent("save002.sav")))
        #expect(movieKind(hard, "movie\\mv_hard") == 13)
        #expect(hard.range(of: Data("movie\\mv_camera".utf8)) == nil)
    }
}
