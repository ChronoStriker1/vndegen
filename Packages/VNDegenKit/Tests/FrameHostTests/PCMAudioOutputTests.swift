import AVFoundation
import Foundation
import Testing
@testable import FrameHost

private func configurationChanged(_ output: PCMAudioOutput) async throws {
    output.engine.stop()
    NotificationCenter.default.post(name: .AVAudioEngineConfigurationChange, object: output.engine)
    try await Task.sleep(for: .milliseconds(100))
}

@Test func soundComesBackAfterTheOutputDeviceChangesUnlessPaused() async throws {
    let output = PCMAudioOutput { out, frames in out.update(repeating: 0, count: frames * 2) }
    output.start()
    guard output.engine.isRunning else { return }
    defer { output.stop() }

    try await configurationChanged(output)
    #expect(output.engine.isRunning)

    output.pause()
    try await configurationChanged(output)
    #expect(!output.engine.isRunning)

    output.resume()
    #expect(output.engine.isRunning)
    output.stop()
    try await configurationChanged(output)
    #expect(!output.engine.isRunning)
}
