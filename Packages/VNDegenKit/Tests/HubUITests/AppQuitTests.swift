import Testing
@testable import HubUI

/// What a quit did, in order.
@MainActor
private final class QuitRecord {
    var events: [String] = []
    let (finished, finish) = AsyncStream<Void>.makeStream()

    func note(_ event: String) {
        events.append(event)
        if event == "reply" || event == "quit again" { finish.yield() }
    }

    func ask(_ quit: AppQuit, fromSystem: Bool = false) -> AppQuit.Answer {
        quit.request(fromSystem: fromSystem, reply: { self.note("reply") }, quitAgain: { self.note("quit again") })
    }

    /// Waits for the quit to finish.
    func waitForFinish() async {
        for await _ in finished { return }
    }
}

/// A gate the test opens to let the quit's preparation finish.
@MainActor
private final class Gate {
    private var waiters: [CheckedContinuation<Void, Never>] = []
    private var isOpen = false

    func wait() async {
        guard !isOpen else { return }
        await withCheckedContinuation { waiters.append($0) }
    }

    func open() {
        isOpen = true
        waiters.forEach { $0.resume() }
        waiters = []
    }
}

@MainActor
@Test func aQuitWaitsForTheGameAndBackupsThenReplies() async {
    let record = QuitRecord()
    let gate = Gate()
    let quit = AppQuit(timeout: .seconds(60), frameIsRunning: { false }) {
        await gate.wait()
        record.note("prepared")
    }
    #expect(record.ask(quit) == .later)
    #expect(record.ask(quit) == .later)
    gate.open()
    await record.waitForFinish()
    #expect(record.events == ["prepared", "reply"])
}

/// AppKit waits for the reply to `.later` inside the call that asked, and a quit that arrived through a game's own
/// dialog would hold the game's frame open there, so the game could never close. Such a quit is cancelled, and the app
/// quits again once the game has stopped and the backups are done.
@MainActor
@Test func aQuitInsideAGameFrameIsCancelledAndMadeAgainOnceTheGameStopped() async {
    let record = QuitRecord()
    let gate = Gate()
    var frameIsRunning = true
    let quit = AppQuit(timeout: .seconds(60), frameIsRunning: { frameIsRunning }) {
        await gate.wait()
        record.note("prepared")
    }
    #expect(record.ask(quit) == .cancel)
    // The cancel lets the frame return.
    frameIsRunning = false
    #expect(record.ask(quit) == .cancel)
    gate.open()
    await record.waitForFinish()
    #expect(record.events == ["prepared", "quit again"])
    #expect(record.ask(quit) == .now)
}

/// Cancelling a quit macOS asks for as it logs out, restarts, or shuts down would cancel the log out, and the next try
/// would quit only VNDegen. It waits inside the frame instead, where the game writes its system saves.
@MainActor
@Test func aQuitMacOSAsksForInsideAGameFrameIsNeverCancelled() async {
    let record = QuitRecord()
    let gate = Gate()
    let quit = AppQuit(timeout: .seconds(60), frameIsRunning: { true }) {
        await gate.wait()
        record.note("prepared")
    }
    #expect(record.ask(quit, fromSystem: true) == .later)
    gate.open()
    await record.waitForFinish()
    #expect(record.events == ["prepared", "reply"])
}

/// A log out that arrives while a cancelled quit waits for the game finishes that quit instead of cancelling.
@MainActor
@Test func aQuitMacOSAsksForAfterACancelledQuitWaitsForIt() async {
    let record = QuitRecord()
    let gate = Gate()
    let quit = AppQuit(timeout: .seconds(60), frameIsRunning: { true }) {
        await gate.wait()
        record.note("prepared")
    }
    #expect(record.ask(quit) == .cancel)
    #expect(record.ask(quit, fromSystem: true) == .later)
    #expect(record.ask(quit, fromSystem: true) == .later)
    gate.open()
    await record.waitForFinish()
    #expect(record.events == ["prepared", "reply"])
}

@MainActor
@Test func aQuitGivesUpWaitingAfterItsTimeout() async {
    let record = QuitRecord()
    let quit = AppQuit(timeout: .milliseconds(50), frameIsRunning: { true }) {
        await Gate().wait()
    }
    #expect(record.ask(quit) == .cancel)
    await record.waitForFinish()
    #expect(record.events == ["quit again"])
    #expect(record.ask(quit) == .now)
}
