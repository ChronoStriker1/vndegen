import Foundation
import SwiftUI
import UniformTypeIdentifiers

/// Files dragged in from another app, such as Files on iPhone and iPad, arrive as item providers holding file
/// representations rather than file URLs.
nonisolated enum DroppedFiles {
    /// Where a drop is copied when it can't be opened in place. `ArchiveImporter` deletes these copies when it closes.
    static let copies = URL.temporaryDirectory.appendingPathComponent("Dropped Archives", isDirectory: true)

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

    static func isCopy(_ url: URL) -> Bool {
        url.standardizedFileURL.path.hasPrefix(copies.standardizedFileURL.path + "/")
    }

    /// The drop's own folder in `copies`.
    static func copyFolder(of url: URL) -> URL { url.deletingLastPathComponent() }

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
                if inPlace { return continuation.resume(returning: url) }
                // The system deletes its copy when this handler returns, so keep it by moving it out first.
                let kept = folder.appendingPathComponent(url.lastPathComponent)
                do {
                    try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
                    do { try FileManager.default.moveItem(at: url, to: kept) } catch { try FileManager.default.copyItem(at: url, to: kept) }
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
    /// `perform`. Pass them to `ArchiveImportView(initialSelection:)` or `ArchiveImporter.choose`.
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
