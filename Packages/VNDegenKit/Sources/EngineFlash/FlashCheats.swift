import Foundation
import VNCore
import WebRuntime

/// Game speed, the one cheat Flash games can have: Ruffle gives no way to reach a movie's variables from outside.
/// flash-page.js runs the movie faster.
final class FlashCheats: CheatSession {
    /// Weak: the page's session owns these cheats.
    private weak var page: (any GamePage)?
    /// flash-page.js with its fonts filled in, before the speed is.
    private let template: String
    /// The page script the session runs now.
    private var script: String

    /// `script` is what `script(_:speed:)` made of `template` for the page's first load.
    init(page: any GamePage, template: String, script: String) {
        self.page = page
        self.template = template
        self.script = script
    }

    let features: CheatFeatures = .speed
    let variableRange = 0...0

    /// Speed works before a game starts too, so the page never asks the player to start one first.
    func state() async throws -> CheatState { CheatState(inGame: true) }

    /// The page offers no actions for a game with speed only.
    func perform(_ action: CheatAction) async throws {}

    /// The page that's running gets the speed now, and a page the game opens next starts with it.
    func apply(_ settings: CheatSettings) {
        let next = Self.script(template, speed: settings.speed)
        page?.replaceStartScript(script, with: next)
        script = next
        Task {
            do {
                _ = try await page?.callFunction("window.__vndegenFlashSpeed && __vndegenFlashSpeed(speed)",
                                                 arguments: ["speed": settings.speed])
            } catch {
                flashLog.error("game speed: \(error.localizedDescription, privacy: .public)")
            }
        }
    }

    /// flash-page.js with `speed` in place.
    nonisolated static func script(_ template: String, speed: Int) -> String {
        template.replacingOccurrences(of: "__VNDEGEN_FLASH_SPEED__", with: String(speed))
    }
}
