import Foundation

/// The helper process that runs a Ren'Py game: the runtime's Python running VNDegen's `launcher.py`, and the environment
/// the launcher and `zz_vndegen.rpy` read.
nonisolated public struct RenPyCommand: Sendable {
    public var executable: URL
    public var arguments: [String]
    public var environment: [String: String]

    /// Where the game window opens.
    public enum Placement: Sendable, Equatable {
        case fullScreen
        /// The window's content area in screen points, with the origin at the top left of the main display, as SDL
        /// counts.
        case frame(CGRect)
    }

    /// The app's bundled runtimes, one folder per runtime named by its raw value. `scripts/renpy-runtime.sh` fetches
    /// them and an Xcode build phase copies them in.
    public static var bundledRuntimes: URL? { Bundle.main.resourceURL?.appendingPathComponent("RenPy", isDirectory: true) }

    /// `launcher.py` and `scripts/zz_vndegen.rpy`.
    @MainActor public static var support: URL? { Bundle.module.url(forResource: "Support", withExtension: nil) }

    public static func python(_ runtime: RenPyRuntime, in runtimes: URL) -> URL {
        runtimes.appendingPathComponent("\(runtime.rawValue)/lib/\(runtime.macFolder)/python")
    }

    public init(base: URL, runtime: RenPyRuntime, runtimes: URL, support: URL, dataDirectory: URL, placement: Placement?) {
        executable = Self.python(runtime, in: runtimes)
        arguments = [support.appendingPathComponent("launcher.py").path, base.path]
        var environment = ProcessInfo.processInfo.environment
        environment["PYTHONHOME"] = nil
        environment["PYTHONPATH"] = nil
        environment["PYTHONDONTWRITEBYTECODE"] = "1"
        environment["VNDEGEN_RENPY_RUNTIME"] = runtimes.appendingPathComponent(runtime.rawValue).path
        environment["VNDEGEN_DATA"] = dataDirectory.path
        switch placement {
        case .fullScreen?:
            environment["VNDEGEN_FULLSCREEN"] = "1"
        case let .frame(rect)?:
            environment["VNDEGEN_WINDOW"] = [rect.minX, rect.minY, rect.width, rect.height].map { String(Int($0.rounded())) }
                .joined(separator: ",")
        case nil:
            break
        }
        self.environment = environment
    }
}
