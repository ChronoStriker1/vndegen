import Foundation
import SwiftUI
import UniformTypeIdentifiers

/// Files dragged in from another app, such as Files on iPhone and iPad, arrive as item providers holding file
/// representations rather than file URLs.
nonisolated enum DroppedFiles {
    /// Where a drop is copied when it can't be opened in place.
    static let copies = URL.temporaryDirectory.appendingPathComponent("Dropped Archives", isDirectory: true)
    #if os(iOS)
    /// Where iOS puts a file another app copies to VNDegen, such as a Mail attachment opened in VNDegen.
    static let inbox = URL.documentsDirectory.appendingPathComponent("Inbox", isDirectory: true)
    #endif

    /// Opens each provider's file in place where its source allows that, so a multi-GB archive is never copied. Other
    /// files are kept in one folder per drop, so the volumes of a split archive stay side by side.
    @MainActor static func urls(from providers: [NSItemProvider]) async -> [URL] {
        let folder = copies.appendingPathComponent(UUID().uuidString, isDirectory: true)
        var urls: [URL] = []
        for provider in providers {
            if let url = await url(from: provider, copyingInto: folder) { urls.append(url) }
        }
        return urls
    }

    /// What to delete once VNDegen is done with `url`, when the system delivered `url` as a copy for VNDegen: the drop's
    /// folder in `copies`, or the file itself in the inbox. Nil for the user's own files, which are read in place.
    static func ownedCopy(of url: URL) -> URL? {
        if url.isInside(copies) { return url.deletingLastPathComponent() }
        #if os(iOS)
        if url.isInside(inbox) { return url }
        #endif
        return nil
    }

    static func discardDropCopies() { discardStale([copies]) }

    /// Deletes the items in `folder` that were added before `date` and that `isHeld` doesn't claim. An item whose added
    /// date can't be read stays, because it may have just arrived.
    @MainActor static func discardCopies(in folder: URL, addedBefore date: Date, keeping isHeld: (URL) -> Bool) {
        let urls = (try? FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: [.addedToDirectoryDateKey])) ?? []
        discardStale(urls.filter { url in
            guard let added = try? url.resourceValues(forKeys: [.addedToDirectoryDateKey]).addedToDirectoryDate else {
                importLog.error("Kept \(url.lastPathComponent, privacy: .public), whose added date can't be read")
                return false
            }
            return added < date && !isHeld(url)
        })
    }

    private static func discardStale(_ urls: [URL]) {
        for url in urls where (try? FileManager.default.removeItem(at: url)) != nil {
            importLog.notice("Deleted the stale copy \(url.lastPathComponent, privacy: .public)")
        }
    }

    static func discard(_ urls: [URL]) {
        for copy in Set(urls.compactMap(ownedCopy)) {
            try? FileManager.default.removeItem(at: copy)
            importLog.notice("Deleted the copy \(copy.lastPathComponent, privacy: .public)")
        }
    }

    @MainActor private static func url(from provider: NSItemProvider, copyingInto folder: URL) async -> URL? {
        // `.data` rather than archive types, because old-style RAR volumes (.r00) and split parts (.001) have no type.
        guard let type = provider.registeredTypeIdentifiers.first(where: { UTType($0)?.conforms(to: .data) == true }) else {
            importLog.error("Drop holds no file, only \(provider.registeredTypeIdentifiers, privacy: .public)")
            return nil
        }
        return await withCheckedContinuation { continuation in
            _ = provider.loadInPlaceFileRepresentation(forTypeIdentifier: type) { @Sendable url, inPlace, error in
                guard let url else {
                    importLog.error("Drop failed to load: \(error?.localizedDescription ?? "no file", privacy: .public)")
                    return continuation.resume(returning: nil)
                }
                importLog.notice("Drop delivered \(url.lastPathComponent, privacy: .public), in place: \(inPlace)")
                // An in-place file is read under coordination when the importer lists and extracts it.
                if inPlace { return continuation.resume(returning: url) }
                // The system deletes its copy when this handler returns, so keep a copy of it. On APFS that is a clone.
                let kept = folder.appendingPathComponent(url.lastPathComponent)
                do {
                    try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
                    try NSFileCoordinator.read([url]) { try FileManager.default.copyItem(at: $0[0], to: kept) }
                    continuation.resume(returning: kept)
                } catch {
                    importLog.error("Drop could not be kept: \(error.localizedDescription, privacy: .public)")
                    continuation.resume(returning: nil)
                }
            }
        }
    }
}

public extension View {
    /// Accepts archives dropped from Finder, or from Files and other apps on iPhone and iPad, and passes their URLs to
    /// `perform`. Some may be copies the system made for VNDegen, so `perform` hands them to
    /// `ArchiveImportView(initialSelection:)`, `ArchiveImportModel.choose`, or `ArchiveImporter.choose`, which delete
    /// those copies when done, or to `ArchiveImporter.discard`.
    func onArchiveDrop(isTargeted: @escaping (Bool) -> Void = { _ in }, perform: @escaping ([URL]) -> Void) -> some View {
        #if os(macOS)
        dropDestination(for: URL.self) { urls, _ in
            let files = urls.filter(\.isFileURL)
            perform(files)
            return !files.isEmpty
        } isTargeted: { isTargeted($0) }
        #else
        onDrop(of: [.data], isTargeted: Binding(get: { false }, set: isTargeted)) { providers in
            Task {
                let urls = await DroppedFiles.urls(from: providers)
                if !urls.isEmpty { perform(urls) }
            }
            return true
        }
        #endif
    }
}

nonisolated extension URL {
    /// Compared with symlinks resolved, because iOS hands out `/private/var` paths for the container at `/var`.
    func isInside(_ folder: URL) -> Bool {
        let folderPath = folder.resolvingSymlinksInPath().standardizedFileURL.path
        let path = resolvingSymlinksInPath().standardizedFileURL.path
        return path.hasPrefix(folderPath.hasSuffix("/") ? folderPath : folderPath + "/")
    }
}
