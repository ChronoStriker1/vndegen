import Foundation
import FrameHost
import VNCore

final class GodotLinks {
    private let ask: (URL, @escaping () -> Void) -> Void
    private var asking = false

    init(ask: @escaping (URL, @escaping () -> Void) -> Void) {
        self.ask = ask
    }

    convenience init(over view: PlatformView) {
        self.init { [weak view] url, done in
            guard let view else { return done() }
            WebLinks.askToOpen(url, over: view, done: done)
        }
    }

    var context: UnsafeMutableRawPointer { Unmanaged.passUnretained(self).toOpaque() }

    nonisolated static let callback: @convention(c) (UnsafePointer<CChar>?, UnsafeMutableRawPointer?) -> Void = { text, context in
        guard let text, let context, let url = WebLinks.page(String(cString: text)) else { return }
        let links = Unmanaged<GodotLinks>.fromOpaque(context).takeUnretainedValue()
        Task { @MainActor [weak links] in links?.open(url) }
    }

    private func open(_ url: URL) {
        guard !asking else { return }
        asking = true
        ask(url) { [weak self] in self?.asking = false }
    }
}
