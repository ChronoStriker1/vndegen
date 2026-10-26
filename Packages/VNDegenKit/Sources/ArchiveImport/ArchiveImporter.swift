import Foundation
import Observation
import os
#if canImport(UIKit)
import UIKit
#endif

/// Picks, drops, and opens, with file names only, so a device report can be read in Console.
nonisolated let importLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "archiveimport")

/// Imports one archive as a new game folder. The archive is extracted into `Games/.importing/<id>` and moved into Games
/// only when everything succeeded, so the library never sees a partial game. The library scan skips dot folders.
@Observable
public final class ArchiveImporter {
    public enum Step: Equatable {
        case choosing
        /// Reading the archive's file list.
        case opening
        /// Showing the archive and the editable folder name.
        case reviewing
        case password(wrong: Bool)
        case extracting
        /// The extraction held only another archive, offered for extraction too.
        case nested(fileName: String)
        case finished(ImportResult)
    }

    public struct ImportResult: Equatable, Sendable {
        /// The new folder in Games. Pass it to the library rescan and focus the game it creates.
        public var folderName: String
        public var files: Int
        public var bytes: Int64
        public var duration: Duration
    }

    public let gamesRoot: URL
    public private(set) var step: Step = .choosing {
        didSet { keepAwake(step == .extracting) }
    }
    /// Why the last action failed, shown on the step it returned to.
    public private(set) var errorMessage: String?
    /// The archive being extracted. After the user accepts a nested archive this is the inner one.
    public private(set) var source: ArchiveSource?
    public private(set) var contents: ArchiveContents?
    public private(set) var progress: ImportProgress?
    public var folderName = ""
    /// Offered only when the picked archive is inside VNDegen's own folders, which no other device or server shares.
    public private(set) var canDeleteArchive = false
    public var deleteArchiveAfterImport = false

    /// The archive the user picked, which "Delete archive after import" removes.
    private var picked: (source: ArchiveSource, contents: ArchiveContents)?
    private var securityScoped: [URL] = []
    /// Folders of dropped files the system could not open in place, deleted when this import closes.
    private var dropCopies: Set<URL> = []
    /// Extraction folders still on disk. A nested import keeps the outer one until the inner one finishes.
    private var stagings: [URL] = []
    /// The outer extraction's game folder while the nested offer is showing.
    private var pendingRoot: URL?
    private var password: String?
    private var task: Task<Void, Never>?
    private var started: ContinuousClock.Instant?
    #if os(macOS)
    private var awakeActivity: NSObjectProtocol?
    #endif

    /// Staging folders of imports running in this process. Any other folder in `.importing` is left from a crash.
    private static var liveStagings: Set<URL> = []

    public init(gamesRoot: URL) {
        self.gamesRoot = gamesRoot
        let leftovers = (try? FileManager.default.contentsOfDirectory(at: stagingParent, includingPropertiesForKeys: nil)) ?? []
        for url in leftovers where !Self.liveStagings.contains(url.standardizedFileURL) {
            try? FileManager.default.removeItem(at: url)
        }
    }

    private var stagingParent: URL { gamesRoot.appendingPathComponent(".importing", isDirectory: true) }

    public var folderNameProblem: String? { GameFolderName.problem(with: folderName, in: gamesRoot) }

    // MARK: Actions

    /// Takes the files from the document picker, open panel, or a drop, and lists the archive.
    public func choose(_ urls: [URL]) async {
        close()
        step = .opening
        securityScoped = urls.filter { $0.startAccessingSecurityScopedResource() }
        dropCopies = Set(urls.filter(DroppedFiles.isCopy).map(DroppedFiles.copyFolder))
        let names = urls.map(\.lastPathComponent)
        importLog.notice("Chose \(urls.count) files \(names, privacy: .public), security-scoped access granted for \(self.securityScoped.count)")
        do {
            let (source, contents) = try await Self.open(urls)
            picked = (source, contents)
            self.source = source
            self.contents = contents
            folderName = source.suggestedFolderName
            // A dropped copy is deleted anyway, and deleting it never frees the original's space.
            canDeleteArchive = dropCopies.isEmpty && source.volumes.allSatisfy(isInOwnFolders)
            step = .reviewing
        } catch {
            importLog.error("Could not open the chosen files: \(String(describing: error), privacy: .public)")
            fail(error, returningTo: .choosing)
        }
    }

    public func startImport() {
        guard task == nil, folderNameProblem == nil, let contents else { return }
        if contents.isEncrypted, password == nil {
            step = .password(wrong: false)
        } else {
            extract()
        }
    }

    public func submitPassword(_ password: String) {
        guard case .password = step else { return }
        self.password = password
        extract()
    }

    /// Stops the extraction in progress and deletes what it wrote, or backs out of the password or nested prompt.
    public func cancel() {
        if let task {
            task.cancel()
        } else {
            pendingRoot = nil
            discardStagings()
            restartFromPicked()
        }
    }

    public func extractNested() {
        guard case .nested = step, let root = pendingRoot, let inner = ArchiveExtractor.nestedArchive(in: root) else { return }
        pendingRoot = nil
        password = nil
        source = inner
        do {
            contents = try ArchiveExtractor.list(inner)
            startImport()
        } catch {
            discardStagings()
            fail(error, returningTo: .reviewing)
            restartFromPicked()
        }
    }

    public func keepNested() {
        guard case .nested = step, let root = pendingRoot else { return }
        pendingRoot = nil
        finish(root)
    }

    /// Goes back to the picker for another archive.
    public func reset() {
        close()
        step = .choosing
    }

    /// Cancels any import and releases the picked files. Call when the import view goes away.
    public func close() {
        task?.cancel()
        task = nil
        discardStagings()
        securityScoped.forEach { $0.stopAccessingSecurityScopedResource() }
        securityScoped = []
        dropCopies.forEach { try? FileManager.default.removeItem(at: $0) }
        dropCopies = []
        picked = nil
        source = nil
        contents = nil
        progress = nil
        password = nil
        errorMessage = nil
        pendingRoot = nil
        deleteArchiveAfterImport = false
    }

    // MARK: Extraction

    private func extract() {
        guard let source, let contents else { return }
        errorMessage = nil
        progress = ImportProgress(total: contents)
        if stagings.isEmpty { started = .now }
        step = .extracting
        let staging = stagingParent.appendingPathComponent(UUID().uuidString, isDirectory: true).standardizedFileURL
        stagings.append(staging)
        Self.liveStagings.insert(staging)
        let password = password
        task = Task {
            do {
                try FileManager.default.createDirectory(at: staging, withIntermediateDirectories: true)
                try ArchiveExtractor.checkSpace(for: contents, on: gamesRoot)
                let extracted = try await Self.extract(source, password: password, into: staging, total: contents) { progress in
                    Task { @MainActor in
                        if self.task != nil { self.progress = progress }
                    }
                }
                // Cancel can land after extraction's last check but before this runs. Nothing may be moved or deleted then.
                try Task.checkCancellation()
                progress = extracted
                task = nil
                let root = ArchiveExtractor.gameRoot(in: staging)
                if let inner = ArchiveExtractor.nestedArchive(in: root) {
                    pendingRoot = root
                    step = .nested(fileName: inner.fileName)
                } else {
                    finish(root)
                }
            } catch {
                task = nil
                try? FileManager.default.removeItem(at: staging)
                stagings.removeAll { $0 == staging }
                Self.liveStagings.remove(staging)
                switch error {
                case ArchiveError.wrongPassword:
                    step = .password(wrong: true)
                case ArchiveError.passwordRequired:
                    step = .password(wrong: false)
                default:
                    discardStagings()
                    fail(error, returningTo: .reviewing)
                    restartFromPicked()
                }
            }
        }
    }

    private func finish(_ root: URL) {
        let name = folderName.trimmingCharacters(in: .whitespaces)
        do {
            // Checked again because another import or the Files app may have taken the name meanwhile.
            if let problem = GameFolderName.problem(with: name, in: gamesRoot) { throw ImportFailure(message: problem) }
            try FileManager.default.moveItem(at: root, to: gamesRoot.appendingPathComponent(name, isDirectory: true))
        } catch {
            discardStagings()
            fail(error, returningTo: .reviewing)
            restartFromPicked()
            return
        }
        discardStagings()
        if deleteArchiveAfterImport, canDeleteArchive, let picked {
            picked.source.volumes.forEach { try? FileManager.default.removeItem(at: $0) }
        }
        let duration = started.map { ContinuousClock.now - $0 } ?? .zero
        step = .finished(ImportResult(
            folderName: name,
            files: progress?.files ?? 0,
            bytes: progress?.bytes ?? 0,
            duration: duration))
    }

    /// After a failure or cancel inside a nested import, the review screen shows the archive the user picked again.
    private func restartFromPicked() {
        guard let picked else { return }
        source = picked.source
        contents = picked.contents
        password = nil
        step = .reviewing
    }

    private func discardStagings() {
        for staging in stagings {
            try? FileManager.default.removeItem(at: staging)
            Self.liveStagings.remove(staging)
        }
        stagings = []
    }

    private func fail(_ error: Error, returningTo step: Step) {
        errorMessage = switch error {
        case let error as ArchiveError: error.message
        case is CancellationError: "Import cancelled. Nothing was added to Games."
        case let error as ImportFailure: error.message
        default: error.localizedDescription
        }
        self.step = step
    }

    private func keepAwake(_ awake: Bool) {
        #if os(iOS)
        UIApplication.shared.isIdleTimerDisabled = awake
        #elseif os(macOS)
        if awake, awakeActivity == nil {
            awakeActivity = ProcessInfo.processInfo.beginActivity(
                options: [.userInitiated, .idleDisplaySleepDisabled], reason: "Importing a game")
        } else if !awake, let activity = awakeActivity {
            ProcessInfo.processInfo.endActivity(activity)
            awakeActivity = nil
        }
        #endif
    }

    // MARK: Off the main actor

    @concurrent
    private static func open(_ urls: [URL]) async throws -> (ArchiveSource, ArchiveContents) {
        let source = try ArchiveSource(picked: urls)
        return (source, try ArchiveExtractor.list(source))
    }

    @concurrent
    private static func extract(
        _ source: ArchiveSource, password: String?, into staging: URL, total: ArchiveContents,
        progress: @escaping @Sendable (ImportProgress) -> Void
    ) async throws -> ImportProgress {
        try ArchiveExtractor.extract(source, password: password, into: staging, total: total, progress: progress)
    }

    /// Deleting a file that iCloud Drive, Dropbox, OneDrive, or a server shares deletes it on the user's other devices
    /// too. A local path can't tell those apart, because File Provider syncs ordinary-looking folders. Only VNDegen's own
    /// folders are certain to be unshared. On iOS that is the app's sandbox container. The Mac app has no sandbox, so
    /// there it is the Games folder.
    private func isInOwnFolders(_ url: URL) -> Bool {
        #if os(iOS)
        let own = URL.homeDirectory
        #else
        let own = gamesRoot
        #endif
        let ownPath = own.resolvingSymlinksInPath().standardizedFileURL.path
        let path = url.resolvingSymlinksInPath().standardizedFileURL.path
        return path.hasPrefix(ownPath.hasSuffix("/") ? ownPath : ownPath + "/")
    }
}

private struct ImportFailure: Error {
    let message: String
}
