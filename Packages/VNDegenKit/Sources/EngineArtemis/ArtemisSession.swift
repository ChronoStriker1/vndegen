import CArtemis
import FontPack
import Foundation
import Metal
import OSLog
import QuartzCore
import VNCore
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

nonisolated let artemisLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "artemis")

/// An Artemis game running on art3m1s-core in VNDegen's process, drawn with Metal in `view`, so the hub's menu and touch
/// controls sit over it. The core runs one frame per display refresh on the main thread. The game starts once the view
/// is on screen. The game quitting itself ends the session through `onExit`.
final class ArtemisSession: EngineSession {
    let view: PlatformView
    private let host: ArtemisHostView
    private let core: ArtemisCore
    private let launch: GameLaunch
    private let layout: ArtemisLayout
    private let files: ArtemisCore.Files
    private let ini: Data
    private let audio: ArtemisAudio
    private let log: FileHandle?
    private let events: OpaquePointer
    private var runtime: OpaquePointer?
    private var stage = CGSize(width: 1280, height: 720)
    private var router = ArtemisInputRouter()
    private var displayLink: CADisplayLink?
    private var lastFrame: CFTimeInterval?
    private var carry = 0.0
    private var paused = false
    private var stopped = false
    /// The last pointer position, in stage pixels, where a click lands.
    private var pointer = CGPoint.zero

    /// Whether a session's runtime is alive. The core keeps some state per process, so one game runs at a time.
    private(set) static var engineInUse = false

    init(_ launch: GameLaunch, layout: ArtemisLayout, core: ArtemisCore) throws {
        self.launch = launch
        self.layout = layout
        self.core = core
        guard let files = ArtemisCore.Files(core: core, folder: launch.folder, layout: layout) else {
            throw ArtemisError.unreadable("archives")
        }
        self.files = files
        guard let ini = files.read("system.ini") else { throw ArtemisError.unreadable("system.ini") }
        self.ini = ini
        let saves = ArtemisPlugin.saveDirectory(launch.dataDirectory)
        let logURL = launch.dataDirectory.appending(path: "logs/artemis.txt")
        try FileManager.default.createDirectory(at: saves, withIntermediateDirectories: true)
        try FileManager.default.createDirectory(at: logURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data().write(to: logURL)
        log = try? FileHandle(forWritingTo: logURL)
        files.setSaveDirectory(saves)
        guard let events = core.api.host_events_create!() else { throw ArtemisError.engineMissing }
        self.events = events
        core.api.host_events_enable!(events, 1)
        audio = ArtemisAudio { [files] path in files.read(path) }
        host = ArtemisHostView()
        view = host
        audio.onFinished = { [weak self] id in
            guard let self, let runtime else { return }
            core.api.runtime_notify_sound_finished!(runtime, id)
        }
        host.onReady = { [weak self] in self?.start() }
        host.onKey = { [weak self] key, pressed, isRepeat in
            guard let self else { return }
            router.hostKey(key, pressed: pressed, isRepeat: isRepeat).forEach(deliver)
        }
        host.onEvent = { [weak self] event in self?.send(event) }
    }

    private func start() {
        guard runtime == nil, !stopped else { return }
        Self.engineInUse = true
        useDefaultFont()
        let settings = ArtemisSettings(ini: ini)
        // Backend 3 is the core's own Metal renderer.
        guard let runtime = core.api.runtime_create!(UInt32(settings.width), UInt32(settings.height), 3) else {
            return fail("the engine could not start")
        }
        self.runtime = runtime
        _ = core.api.runtime_set_resources!(runtime, files.resources)
        _ = core.api.runtime_set_pc_save_version!(runtime, layout.saveVersion)
        let loaded = ini.withUnsafeBytes { bytes in
            core.api.runtime_load_project_bytes!(runtime, bytes.baseAddress?.assumingMemoryBound(to: UInt8.self), bytes.count, "WINDOWS")
        }
        guard loaded == 0 else { return fail("the game's project could not be loaded") }
        stage = CGSize(width: Int(core.api.runtime_stage_width!(runtime)), height: Int(core.api.runtime_stage_height!(runtime)))
        let layer = host.metalLayer
        layer.device = MTLCreateSystemDefaultDevice()
        layer.pixelFormat = .bgra8Unorm
        layer.drawableSize = stage
        // Surface kind 4 is a CAMetalLayer the core presents to.
        let surface = core.api.runtime_set_external_surface!(runtime, 4, Unmanaged.passUnretained(layer).toOpaque(),
                                                            UInt32(stage.width), UInt32(stage.height))
        // 1 means the core took the layer.
        guard surface == 1 else { return fail("the game's view could not be set up") }
        drainEvents()
        artemisLog.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        if paused { audio.setPaused(true) }
        host.takeFocus()
        let target = FrameTarget(self)
        #if canImport(UIKit)
        let link = CADisplayLink(target: target, selector: #selector(FrameTarget.step))
        #else
        let link = host.displayLink(target: target, selector: #selector(FrameTarget.step))
        #endif
        link.add(to: .main, forMode: .common)
        displayLink = link
    }

    /// The core draws text in the fonts a game ships. A face the game ships no font file for, such as a Windows font
    /// or the non-ASCII characters of a bitmap `.rft` font, uses this one, the font pack's MS Gothic.
    private func useDefaultFont() {
        guard let match = FontPack.resolve("MS Gothic"), let data = try? Data(contentsOf: match.file) else { return }
        _ = data.withUnsafeBytes { bytes in
            core.api.set_font_override!(bytes.baseAddress?.assumingMemoryBound(to: UInt8.self), Int32(bytes.count))
        }
    }

    private func fail(_ reason: String) {
        artemisLog.error("\(self.launch.game.id.rawValue, privacy: .public): \(reason, privacy: .public)")
        writeLog("[E] \(reason)")
        finish()
        launch.onExit()
    }

    func step() {
        guard let runtime, let link = displayLink, !paused else { return }
        let now = link.timestamp
        let elapsed = lastFrame.map { min((now - $0) * 1000, 100) } ?? 16
        lastFrame = now
        let total = elapsed + carry
        let delta = UInt32(total)
        carry = total - Double(delta)
        _ = core.api.runtime_advance_and_present!(runtime, delta)
        drainEvents()
        audio.advance(Double(delta))
        if core.api.runtime_is_exit_requested!(runtime) != 0 {
            finish()
            launch.onExit()
        }
    }

    // MARK: Host events

    private func drainEvents() {
        while true {
            let next = core.api.host_events_next!(events)
            guard next > 0 else { return }
            var buffer = Data(count: next)
            var count: UInt32 = 0
            let written = buffer.withUnsafeMutableBytes { bytes in
                core.api.poll_events!(events, bytes.baseAddress?.assumingMemoryBound(to: UInt8.self), next, &count)
            }
            guard written > 0, count > 0 else { return }
            var offset = 0
            let header = MemoryLayout<ArtHostEventHeaderV1>.size
            for _ in 0..<count where offset + header <= written {
                let event = buffer.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: offset, as: ArtHostEventHeaderV1.self) }
                offset += header
                let end = offset + Int(event.payload_size)
                guard end <= written else { return }
                handle(kind: event.kind, level: event.aux, payload: buffer.subdata(in: offset..<end))
                offset = end
            }
        }
    }

    /// Kind 1 is a log line, 2 a media command, and 3 a request to the host's interface.
    private func handle(kind: UInt32, level: UInt32, payload: Data) {
        guard kind != 1 else {
            return writeLog("[\(UnicodeScalar(UInt8(truncatingIfNeeded: level)).escaped(asASCII: true))] \(String(decoding: payload, as: UTF8.self))")
        }
        guard let command = try? JSONSerialization.jsonObject(with: payload) as? [String: Any],
              let name = command["kind"] as? String else { return }
        let body = command["payload"] as? [String: Any] ?? [:]
        switch name {
        case "video_play":
            // Videos don't play yet. A fullscreen one, which the script waits on, ends at once.
            writeLog("[W] video not played: \(body["file"] as? String ?? "")")
            if body["id"] == nil || body["id"] is NSNull, let runtime { core.api.runtime_notify_video_finished!(runtime, nil) }
        case "dialog_show":
            showDialog(body)
        case "openbrowser":
            if let link = body["url"] as? String, let url = URL(string: link) { open(url) }
        default:
            if name.hasPrefix("audio_") { audio.handle(name, body) }
        }
    }

    private func writeLog(_ line: String) {
        log?.write(Data((line + "\n").utf8))
    }

    /// The game's own Yes/No or text dialog, which the script waits on until it is answered.
    private func showDialog(_ body: [String: Any]) {
        let title = body["title"] as? String ?? "", message = body["message"] as? String ?? ""
        let cancels = body["hasCancel"] as? Bool ?? false, asksText = body["textfield"] as? Bool ?? false
        let initial = body["initialText"] as? String ?? ""
        let answer: @MainActor (Bool, String?) -> Void = { [weak self] accepted, text in
            guard let self, let runtime else { return }
            _ = core.api.runtime_submit_dialog!(runtime, accepted ? 1 : 0, text)
        }
        #if canImport(UIKit)
        let alert = UIAlertController(title: title, message: message, preferredStyle: .alert)
        if asksText { alert.addTextField { $0.text = initial } }
        alert.addAction(UIAlertAction(title: "OK", style: .default) { [weak alert] _ in answer(true, alert?.textFields?.first?.text) })
        if cancels { alert.addAction(UIAlertAction(title: "Cancel", style: .cancel) { _ in answer(false, nil) }) }
        var presenter = host.window?.rootViewController
        while let shown = presenter?.presentedViewController { presenter = shown }
        presenter?.present(alert, animated: true)
        #else
        let alert = NSAlert()
        alert.messageText = title.isEmpty ? message : title
        if !title.isEmpty { alert.informativeText = message }
        let field = asksText ? NSTextField(string: initial) : nil
        field?.frame = NSRect(x: 0, y: 0, width: 260, height: 24)
        alert.accessoryView = field
        alert.addButton(withTitle: "OK")
        if cancels { alert.addButton(withTitle: "Cancel") }
        guard let window = host.window else { return answer(alert.runModal() == .alertFirstButtonReturn, field?.stringValue) }
        alert.beginSheetModal(for: window) { response in
            MainActor.assumeIsolated { answer(response == .alertFirstButtonReturn, field?.stringValue) }
        }
        #endif
    }

    private func open(_ url: URL) {
        #if canImport(UIKit)
        UIApplication.shared.open(url)
        #else
        NSWorkspace.shared.open(url)
        #endif
    }

    // MARK: Input

    private func deliver(_ input: ArtemisInput) {
        guard let runtime else { return }
        switch input {
        case let .key(key, down):
            core.api.runtime_feed_key!(runtime, UInt32(key), down ? 1 : 0)
        case let .mouseMove(to):
            movePointer(to, runtime)
        case let .mouseButton(button, down, at):
            movePointer(at, runtime)
            core.api.runtime_feed_mouse_button!(runtime, UInt32(button), down ? 1 : 0)
        case let .click(button, down):
            core.api.runtime_feed_mouse_button!(runtime, UInt32(button), down ? 1 : 0)
        }
    }

    /// A point normalized over the view, as a stage pixel. The stage fills the view's middle, with bars beside or
    /// above it.
    private func movePointer(_ location: CGPoint, _ runtime: OpaquePointer) {
        let bounds = host.bounds.size
        guard bounds.width > 0, bounds.height > 0, stage.width > 0, stage.height > 0 else { return }
        let scale = min(bounds.width / stage.width, bounds.height / stage.height)
        let left = (bounds.width - stage.width * scale) / 2, top = (bounds.height - stage.height * scale) / 2
        pointer = CGPoint(x: (location.x * bounds.width - left) / scale, y: (location.y * bounds.height - top) / scale)
        core.api.runtime_feed_mouse!(runtime, Int32(pointer.x.rounded()), Int32(pointer.y.rounded()))
    }

    // MARK: EngineSession

    func send(_ event: ControllerEvent) {
        guard runtime != nil, !paused else { return }
        router.translate(event).forEach(deliver)
    }

    func setPaused(_ paused: Bool) {
        self.paused = paused
        // The hub's menu takes the keyboard, so the game would never see those keys come up.
        if paused { router.releaseAll().forEach(deliver) }
        audio.setPaused(paused)
        lastFrame = nil
        if let runtime { core.api.runtime_notify_lifecycle!(runtime, paused ? 1 : 2) }
        if !paused { host.takeFocus() }
    }

    func terminate() {
        finish()
    }

    private func finish() {
        guard !stopped else { return }
        stopped = true
        displayLink?.invalidate()
        displayLink = nil
        audio.stopAll()
        if let runtime {
            // A game that saves on exit, through [autosave allow=1], saves now.
            core.api.runtime_notify_lifecycle!(runtime, 0)
            core.api.runtime_destroy!(runtime)
            self.runtime = nil
        }
        core.api.host_events_destroy!(events)
        try? log?.close()
        Self.engineInUse = false
        artemisLog.info("\(self.launch.game.id.rawValue, privacy: .public) stopped")
    }
}

/// A display link keeps its target, so the session sits behind a weak link to avoid a cycle.
private final class FrameTarget: NSObject {
    private weak var session: ArtemisSession?

    init(_ session: ArtemisSession) {
        self.session = session
    }

    @objc func step() { session?.step() }
}
