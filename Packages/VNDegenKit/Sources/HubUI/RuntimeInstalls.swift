import Foundation
import Observation
import RuntimePackages
import VNCore

/// Runtime packages, such as the RPG Maker RTPs, that games in the library need: which ones a game is missing, and the
/// installs the player started from a game's page. One install per package runs at a time, and every game shares it.
@MainActor
@Observable
final class RuntimeInstalls {
    enum Phase: Equatable {
        case running(RuntimeInstallProgress)
        case failed(String)
    }

    /// The installs started or failed since the app launched, by package ID. A finished install leaves no entry.
    private(set) var phases: [String: Phase] = [:]
    /// The packages each game needs and doesn't have, as of the last look at its page.
    private(set) var missing: [GameID: [RuntimePackage]] = [:]
    /// Shows the license an install waits on. The navigator presents it as a sheet.
    @ObservationIgnored var onLicense: ((LicenseRequest) -> Void)?
    /// The package whose folder the player is picking, and whether the game page's folder picker shows. SwiftUI clears
    /// the picker's flag before it reports the folder, so the package is kept apart from it.
    var folderImport: RuntimePackage?
    var pickingFolder = false

    func pickFolder(for package: RuntimePackage) {
        folderImport = package
        pickingFolder = true
    }

    @ObservationIgnored private let library: LibraryStore
    @ObservationIgnored private var tasks: [String: Task<Void, Never>] = [:]

    init(library: LibraryStore) {
        self.library = library
    }

    var store: RuntimePackageStore { RuntimePackageStore(root: library.paths.runtimesDirectory) }

    /// Works out which packages a game is missing. Reads the game's folder, so it runs off the main thread.
    func refresh(_ id: GameID) async {
        guard let game = library.game(id), let plugin = library.plugin(for: game.engine) else {
            missing[id] = nil
            return
        }
        let folder = library.paths.folder(for: id), store = store
        missing[id] = await Task.detached {
            plugin.runtimePackages(for: game, folder: folder).filter { store.installedFolder(for: $0) == nil }
        }.value
    }

    func install(_ package: RuntimePackage, for game: GameID) {
        guard tasks[package.id] == nil else { return }
        phases[package.id] = .running(.downloading(received: 0, total: package.size))
        let store = store
        tasks[package.id] = Task { [weak self] in
            do {
                _ = try await store.install(package, progress: { [weak self] progress in
                    Task { @MainActor in self?.update(package, progress) }
                }, acceptLicense: { [weak self] text in
                    await self?.askToAccept(package, text: text) ?? false
                })
                self?.finished(package, game: game, error: nil)
            } catch {
                self?.finished(package, game: game, error: error)
            }
        }
    }

    private func update(_ package: RuntimePackage, _ progress: RuntimeInstallProgress) {
        guard case .running = phases[package.id] else { return }
        phases[package.id] = .running(progress)
    }

    func cancel(_ package: RuntimePackage) {
        tasks[package.id]?.cancel()
    }

    /// Copies a folder the player picked, such as an RTP installed on a PC, as the package.
    func importFolder(_ url: URL, as package: RuntimePackage, for game: GameID) {
        let store = store
        phases[package.id] = .running(.installing)
        Task {
            let error: Error? = await Task.detached {
                let scoped = url.startAccessingSecurityScopedResource()
                defer { if scoped { url.stopAccessingSecurityScopedResource() } }
                do {
                    _ = try store.importFolder(url, as: package)
                    return nil
                } catch {
                    return error
                }
            }.value
            finished(package, game: game, error: error)
        }
    }

    private func finished(_ package: RuntimePackage, game: GameID, error: Error?) {
        tasks[package.id] = nil
        if let error, !(error is CancellationError), error as? RuntimeInstallError != .declinedLicense {
            phases[package.id] = .failed(Self.describe(error, package))
        } else {
            phases[package.id] = nil
        }
        Task { await refresh(game) }
    }

    private func askToAccept(_ package: RuntimePackage, text: String?) async -> Bool {
        await withCheckedContinuation { continuation in
            let request = LicenseRequest(package: package, text: text) { continuation.resume(returning: $0) }
            if let onLicense { onLicense(request) } else { request.respond(false) }
        }
    }

    static func describe(_ error: Error, _ package: RuntimePackage) -> String {
        switch error as? RuntimeInstallError {
        case .checksumMismatch?, .sizeMismatch?:
            "The file downloaded from \(package.download.host ?? "the publisher") is not the one VNDegen expects, so it was deleted. The publisher may have changed it."
        case let .httpStatus(status)?:
            "The download failed with HTTP status \(status)."
        case let .unsupportedInstaller(reason)?, let .corrupt(reason)?:
            "The installer could not be unpacked: \(reason)."
        case .declinedLicense?, nil:
            error.localizedDescription
        }
    }
}

/// An install waiting for the player to accept the package's license, which the official installer shows too.
final class LicenseRequest: Identifiable {
    let package: RuntimePackage
    /// The license the installer carries, or nil when it carries none and the publisher's page has it.
    let text: String?
    private var answer: ((Bool) -> Void)?

    init(package: RuntimePackage, text: String?, answer: @escaping (Bool) -> Void) {
        self.package = package
        self.text = text
        self.answer = answer
    }

    var id: String { package.id }

    /// Answers once. Later answers, such as the sheet closing after Accept, do nothing.
    func respond(_ accepted: Bool) {
        answer?(accepted)
        answer = nil
    }
}
