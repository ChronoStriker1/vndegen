import Foundation
import VNCore

public enum RuntimeInstallProgress: Sendable, Equatable {
    case downloading(received: Int64, total: Int64)
    case verifying
    /// Counts files written so far out of all the package's files.
    case extracting(done: Int, total: Int)
    case installing
}

public enum RuntimeInstallError: Error, Equatable {
    case checksumMismatch(expected: String, actual: String)
    case sizeMismatch(expected: Int64, actual: Int64)
    case httpStatus(Int)
    case unsupportedInstaller(String)
    case declinedLicense
    case corrupt(String)
}

/// Installed runtime packages, one folder per package under `root`, shared by every game that needs one.
///
/// Layout: `root/<package id>/` holds the package's files. `root/.receipts/<package id>` exists only while that folder
/// is complete, so an install interrupted at any point reads as not installed. Work happens in `root/.staging/`, on the
/// same volume, so finished files move into place by rename.
public struct RuntimePackageStore: Sendable {
    public let root: URL

    public init(root: URL) {
        self.root = root
    }

    /// The installed package's folder, holding the installer's {app} files at its top level, or nil when not (fully)
    /// installed.
    public func installedFolder(for package: RuntimePackage) -> URL? {
        let folder = folder(for: package)
        let manager = FileManager.default
        guard manager.fileExists(atPath: receipt(for: package).path), manager.fileExists(atPath: folder.path) else {
            return nil
        }
        return folder
    }

    /// Downloads, verifies, asks for license acceptance (Inno Setup installers only), extracts, installs atomically.
    /// Throws CancellationError when the task is cancelled, and leaves no partial files behind on any failure.
    public func install(_ package: RuntimePackage, session: URLSession = .shared,
                        progress: @escaping @Sendable (RuntimeInstallProgress) -> Void,
                        acceptLicense: @escaping @Sendable (_ licenseText: String?) async -> Bool) async throws -> URL {
        let work = try makeWorkFolder()
        defer { try? FileManager.default.removeItem(at: work) }

        let fileExtension = package.download.pathExtension
        let download = work.appending(path: fileExtension.isEmpty ? "download" : "download.\(fileExtension)")
        try await PackageDownload.fetch(package, to: download, session: session, progress: progress)

        let files = work.appending(path: "files")
        try FileManager.default.createDirectory(at: files, withIntermediateDirectories: false)
        let setup: URL
        switch package.installer {
        case .zip(let members):
            try ZipInstaller.extract(download, members: members, to: files) { done, total in
                progress(.extracting(done: done, total: total))
            }
            try Task.checkCancellation()
            progress(.installing)
            return try commit(files, as: package, work: work, receiptText: package.sha256)
        case .innoSetup(let path?):
            setup = try ZipInstaller.extract(download, setupPath: path, to: work.appending(path: "installer"))
            try FileManager.default.removeItem(at: download)
        case .innoSetup(nil):
            setup = download
        }
        let installer = try InnoSetupInstaller(setup: setup)
        try Task.checkCancellation()
        guard await acceptLicense(installer.licenseText) else { throw RuntimeInstallError.declinedLicense }
        try Task.checkCancellation()

        try installer.extractAppFiles(to: files) { done, total in progress(.extracting(done: done, total: total)) }
        try Task.checkCancellation()
        progress(.installing)
        return try commit(files, as: package, work: work, receiptText: package.sha256)
    }

    /// Copies a folder the player picked (an RTP installed on a PC, say) as the package. Replaces any earlier install.
    /// The caller holds any security-scoped access `source` needs.
    public func importFolder(_ source: URL, as package: RuntimePackage) throws -> URL {
        let work = try makeWorkFolder()
        defer { try? FileManager.default.removeItem(at: work) }
        let files = work.appending(path: "files")
        try FileManager.default.copyItem(at: source, to: files)
        return try commit(files, as: package, work: work, receiptText: "imported")
    }

    public func remove(_ package: RuntimePackage) throws {
        let manager = FileManager.default
        for item in [receipt(for: package), folder(for: package)] where manager.fileExists(atPath: item.path) {
            try manager.removeItem(at: item)
        }
    }

    private func folder(for package: RuntimePackage) -> URL {
        root.appending(path: package.id, directoryHint: .isDirectory)
    }

    private func receipt(for package: RuntimePackage) -> URL {
        root.appending(path: ".receipts").appending(path: package.id)
    }

    /// A fresh folder under `.staging`. Folders there older than a day are left from an install the app didn't live to
    /// finish, so they go; younger ones may belong to an install still running.
    private func makeWorkFolder() throws -> URL {
        let manager = FileManager.default
        let staging = root.appending(path: ".staging", directoryHint: .isDirectory)
        let stale = Date.now.addingTimeInterval(-24 * 60 * 60)
        for folder in (try? manager.contentsOfDirectory(at: staging, includingPropertiesForKeys: [.contentModificationDateKey])) ?? [] {
            if let modified = try? folder.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate,
               modified < stale {
                try? manager.removeItem(at: folder)
            }
        }
        let work = staging.appending(path: UUID().uuidString, directoryHint: .isDirectory)
        try manager.createDirectory(at: work, withIntermediateDirectories: true)
        return work
    }

    /// Moves finished `files` into place. An earlier install stays until the new files are ready, then moves aside
    /// into `work`, which the caller deletes. The receipt goes first and comes back last, so a crash in between leaves
    /// the package not installed rather than half replaced.
    private func commit(_ files: URL, as package: RuntimePackage, work: URL, receiptText: String) throws -> URL {
        let manager = FileManager.default
        let folder = folder(for: package)
        let receipt = receipt(for: package)
        try manager.createDirectory(at: receipt.deletingLastPathComponent(), withIntermediateDirectories: true)
        let previousReceipt = try? Data(contentsOf: receipt)
        if previousReceipt != nil { try manager.removeItem(at: receipt) }
        let previous = work.appending(path: "previous")
        let replacing = manager.fileExists(atPath: folder.path)
        if replacing { try manager.moveItem(at: folder, to: previous) }
        do {
            try manager.moveItem(at: files, to: folder)
        } catch {
            if replacing, (try? manager.moveItem(at: previous, to: folder)) != nil, let previousReceipt {
                try? previousReceipt.write(to: receipt)
            }
            throw error
        }
        try Data(receiptText.utf8).write(to: receipt)
        return folder
    }
}
