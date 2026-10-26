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
        /// Volumes "Delete archive after import" left in place because another file replaced them after they were chosen.
        public var keptArchives: [String] = []
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
    /// On by default for an archive the library lists under Ready to import, which the user copied in only to import it.
    public var deleteArchiveAfterImport = false

    /// The archive the user picked, which "Delete archive after import" removes, and each volume's identity when it was
    /// listed. A volume whose file changed since then is a different file and stays.
    private var picked: (source: ArchiveSource, contents: ArchiveContents, identities: [FileIdentity?])?
    /// The current choice. A listing or extraction whose selection was replaced has nothing left to show.
    private var selection: Selection?
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

    /// Deletes the copies the system made for VNDegen among `urls`, for files from a drop or Open in VNDegen that
    /// nothing imports.
    public static func discard(_ urls: [URL]) { DroppedFiles.discard(urls) }

    /// Deletes the drop copies left by a run that was killed mid-import. Call at launch, before any drop can arrive.
    public static func discardStaleDropCopies() { DroppedFiles.discardDropCopies() }

    /// Deletes the Inbox copies that no import reads and that were already in the Inbox at `date`. Pass the time the app
    /// last went to the background, so a copy has sat through a whole foreground session unclaimed. A copy delivered for
    /// Open in VNDegen arrives before its URL callback, at a cold launch even before the app starts, so a newer one may
    /// still be on its way to an import sheet.
    public static func discardInboxCopies(addedBefore date: Date) {
        #if os(iOS)
        discardCopies(in: DroppedFiles.inbox, addedBefore: date)
        #endif
    }

    /// `discardInboxCopies` for any folder.
    static func discardCopies(in folder: URL, addedBefore date: Date) {
        DroppedFiles.discardCopies(in: folder, addedBefore: date, keeping: Selection.isHeld)
    }

    // MARK: Actions

    /// Takes the files from the document picker, open panel, a drop, or Open in VNDegen, and lists the archive. The
    /// importer owns them from here and deletes the ones the system copied for VNDegen when it closes.
    public func choose(_ urls: [URL]) async {
        close()
        let selection = Selection(urls)
        self.selection = selection
        step = .opening
        let names = urls.map(\.lastPathComponent)
        importLog.notice("Chose \(urls.count) files \(names, privacy: .public), security-scoped access granted for \(selection.scopedCount)")
        do {
            let (source, contents, identities) = try await selection.read { try await Self.open(urls) }
            // Sibling volumes found next to the picked file need the same protection from Inbox cleanup.
            selection.hold(source.volumes)
            guard self.selection === selection else {
                return importLog.notice("Listed \(names, privacy: .public) after a newer choice replaced it")
            }
            picked = (source, contents, identities)
            self.source = source
            self.contents = contents
            folderName = source.suggestedFolderName
            // A copy is deleted anyway, and deleting it never frees the original's space.
            canDeleteArchive = source.volumes.allSatisfy { DroppedFiles.ownedCopy(of: $0) == nil && isInOwnFolders($0) }
            deleteArchiveAfterImport = canDeleteArchive && source.volumes.allSatisfy { LocalArchives.isListed($0, gamesRoot: gamesRoot) }
            step = .reviewing
        } catch {
            guard self.selection === selection else {
                return importLog.notice("Could not list \(names, privacy: .public), which a newer choice replaced")
            }
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
        selection?.close()
        selection = nil
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
        guard let selection, let source, let contents else { return }
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
                let extracted = try await selection.read {
                    try await Self.extract(source, password: password, into: staging, total: contents) { progress in
                        Task { @MainActor in
                            if self.selection === selection, self.task != nil { self.progress = progress }
                        }
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
                try? FileManager.default.removeItem(at: staging)
                Self.liveStagings.remove(staging)
                guard self.selection === selection else { return }
                task = nil
                stagings.removeAll { $0 == staging }
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
        var kept: [String] = []
        if deleteArchiveAfterImport, canDeleteArchive, let picked {
            for (volume, identity) in zip(picked.source.volumes, picked.identities) {
                if let identity, NSFileCoordinator.delete(volume, ifStill: identity) { continue }
                importLog.notice("Kept \(volume.lastPathComponent, privacy: .public), which changed after it was chosen")
                kept.append(volume.lastPathComponent)
            }
        }
        let duration = started.map { ContinuousClock.now - $0 } ?? .zero
        step = .finished(ImportResult(
            folderName: name,
            files: progress?.files ?? 0,
            bytes: progress?.bytes ?? 0,
            duration: duration,
            keptArchives: kept))
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
    private static func open(_ urls: [URL]) async throws -> (ArchiveSource, ArchiveContents, [FileIdentity?]) {
        let source = try ArchiveSource(picked: urls)
        // The coordinator hands out where the archive is now, which can differ if it moved while the read waited.
        return try source.coordinatedRead { granted in
            (granted, try ArchiveExtractor.list(granted), granted.volumes.map(FileIdentity.init(of:)))
        }
    }

    @concurrent
    private static func extract(
        _ source: ArchiveSource, password: String?, into staging: URL, total: ArchiveContents,
        progress: @escaping @Sendable (ImportProgress) -> Void
    ) async throws -> ImportProgress {
        try source.coordinatedRead {
            try ArchiveExtractor.extract($0, password: password, into: staging, total: total, progress: progress)
        }
    }

    /// Deleting a file that iCloud Drive, Dropbox, OneDrive, or a server shares deletes it on the user's other devices
    /// too. A local path can't tell those apart, because File Provider syncs ordinary-looking folders. Only VNDegen's own
    /// folders are certain to be unshared. On iOS that is the app's sandbox container. The Mac app has no sandbox, so
    /// there it is the Games folder.
    private func isInOwnFolders(_ url: URL) -> Bool {
        #if os(iOS)
        url.isInside(URL.homeDirectory)
        #else
        url.isInside(gamesRoot)
        #endif
    }
}

/// The files of one `choose` and what they hold until nothing reads them: security-scoped access, and the copies the
/// system made for VNDegen. A newer choice can replace this one while its listing or extraction still reads, so the
/// files are released when the last read finishes rather than when the selection closes.
private final class Selection {
    /// Selections whose files are not released yet, whatever happened to the sheet that chose them.
    private static var live: [Selection] = []

    private var urls: [URL]
    private let scoped: [URL]
    private var readers = 0
    private var closed = false
    private var released = false

    var scopedCount: Int { scoped.count }

    init(_ urls: [URL]) {
        self.urls = urls
        scoped = urls.filter { $0.startAccessingSecurityScopedResource() }
        Self.live.append(self)
    }

    /// Adds files the selection reads beyond the ones it was given, such as the other volumes of a split archive.
    func hold(_ more: [URL]) {
        urls += more.filter { added in !urls.contains { $0.isSamePath(as: added) } }
    }

    /// Whether a selection still holds `url`.
    static func isHeld(_ url: URL) -> Bool {
        live.contains { $0.urls.contains { $0.isSamePath(as: url) } }
    }

    func read<T>(_ body: () async throws -> T) async throws -> T {
        readers += 1
        defer {
            readers -= 1
            releaseIfUnused()
        }
        return try await body()
    }

    func close() {
        closed = true
        releaseIfUnused()
    }

    private func releaseIfUnused() {
        guard closed, readers == 0, !released else { return }
        released = true
        Self.live.removeAll { $0 === self }
        scoped.forEach { $0.stopAccessingSecurityScopedResource() }
        DroppedFiles.discard(urls)
    }
}

private struct ImportFailure: Error {
    let message: String
}
