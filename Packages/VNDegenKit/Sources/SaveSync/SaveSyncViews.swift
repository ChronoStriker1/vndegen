import SwiftUI
import UniformTypeIdentifiers
import VNCore

/// Ludusavi folder and automatic backup, shown from the app's Settings screen. It draws a plain grouped form and takes
/// its colors from the host's color scheme and tint.
public struct SaveSyncSettingsView: View {
    @Bindable var service: SaveSyncService
    @State private var picking = false

    public init(service: SaveSyncService) { self.service = service }

    public var body: some View {
        Form {
            Section {
                switch service.folder {
                case .none:
                    Text("No folder chosen").foregroundStyle(.secondary)
                case let .ready(url):
                    FolderRow(url: url, detail: service.gameFolders.count == 1 ? "1 game backed up" : "\(service.gameFolders.count) games backed up")
                case let .unavailable(message):
                    Label(message, systemImage: "exclamationmark.triangle.fill").foregroundStyle(.orange)
                }
                Button(service.folder == .none ? "Choose Folder…" : "Choose Another Folder…", systemImage: "folder") { picking = true }
                if let suggested = service.suggestedFolder, service.folder != .ready(suggested) {
                    Button { service.chooseFolder(suggested) } label: {
                        FolderRow(url: suggested, detail: "Use Ludusavi's backup folder")
                    }
                    .buttonStyle(.plain)
                }
                if service.folder != .none {
                    // Hosts may set a plain foreground style, which would otherwise hide the destructive red.
                    Button("Forget Folder", systemImage: "xmark.circle", role: .destructive) { service.forgetFolder() }
                        .foregroundStyle(.red)
                }
            } header: {
                Text("Backup folder")
            } footer: {
                Text("Pick the folder Ludusavi backs up to. For a folder on a server, connect to the server in the Files app first.")
            }

            retention

            Section {
                Toggle("Back up when a game closes", isOn: $service.autoBackup)
            } footer: {
                Text("Adds a new Ludusavi backup each time you quit a game that has one. Existing backups are never changed.")
            }
        }
        .formStyle(.grouped)
        .fileImporter(isPresented: $picking, allowedContentTypes: [.folder]) { result in
            if case let .success(url) = result { service.chooseFolder(url) }
        }
        .task(id: service.folder) { await service.refresh() }
    }
}

extension SaveSyncSettingsView {
    /// Decides between full and differential backups the way Ludusavi would.
    @ViewBuilder private var retention: some View {
        Section {
            if let limits = service.configRetention {
                LabeledContent("Full backups", value: "\(limits.full)")
                LabeledContent("Differential backups", value: "\(limits.differential)")
            } else {
                Toggle("Match my Ludusavi settings", isOn: Binding(
                    get: { service.manualRetention != nil },
                    set: { service.manualRetention = $0 ? Retention(full: 1, differential: 1) : nil }))
                if let limits = service.manualRetention {
                    Stepper("Full backups: \(limits.full)", value: Binding(
                        get: { limits.full }, set: { service.manualRetention?.full = $0 }), in: 1...255)
                    Stepper("Differential backups: \(limits.differential)", value: Binding(
                        get: { limits.differential }, set: { service.manualRetention?.differential = $0 }), in: 0...255)
                }
            }
        } header: {
            Text("Retention")
        } footer: {
            Text(service.configRetention != nil
                 ? "From Ludusavi's settings on this Mac. New backups are full or differential as Ludusavi would make them."
                 : "Enter the limits from Ludusavi's Other screen so new backups are full or differential as Ludusavi would make them. When off, VNDegen follows the backups already in the folder.")
        }
    }
}

private struct FolderRow: View {
    let url: URL
    let detail: String

    var body: some View {
        Label {
            VStack(alignment: .leading, spacing: 2) {
                Text(url.lastPathComponent).font(.body.weight(.semibold))
                Text(detail).font(.caption).foregroundStyle(.secondary)
                Text(url.path).font(.caption2.monospaced()).foregroundStyle(.tertiary).lineLimit(1).truncationMode(.middle)
            }
        } icon: {
            Image(systemName: "externaldrive.fill").foregroundStyle(.tint)
        }
        .contentShape(.rect)
    }
}

/// A game's Ludusavi backups, with restore, back up, and undo. Shown from the game page, styled like
/// `SaveSyncSettingsView`.
public struct GameSavesView: View {
    let service: SaveSyncService
    let game: GameID
    @State private var backups: [BackupSummary] = []
    @State private var selected: String?
    @State private var busy = false
    @State private var status: Status?
    @State private var confirming: Confirmation?
    @State private var choosingGame = false

    public init(service: SaveSyncService, game: GameID) {
        self.service = service
        self.game = game
    }

    private struct Status {
        var text: String
        var isError: Bool
    }

    private enum Confirmation: Identifiable {
        case restore(String), undo
        var id: String { if case let .restore(name) = self { name } else { "undo" } }
    }

    public var body: some View {
        Form {
            if case .ready = service.folder {
                Section("Ludusavi game") {
                    Button { choosingGame = true } label: {
                        HStack {
                            Text("Backups from")
                            Spacer()
                            Text(service.match(for: game) ?? "No match").foregroundStyle(.secondary).lineLimit(1)
                            Image(systemName: "chevron.right").font(.footnote.weight(.semibold)).foregroundStyle(.tertiary)
                        }
                        .contentShape(.rect)
                    }
                    .buttonStyle(.plain)
                }
                backupList
                actions
            } else {
                Section {
                    Text("Choose your Ludusavi backup folder in Settings to restore and back up this game's saves.")
                        .foregroundStyle(.secondary)
                }
            }
        }
        .formStyle(.grouped)
        .disabled(busy)
        .task(id: service.folder) {
            await service.refresh()
            await load()
        }
        .confirmationDialog(confirmationTitle, isPresented: .init(get: { confirming != nil }, set: { if !$0 { confirming = nil } }),
                            titleVisibility: .visible, presenting: confirming) { confirmation in
            switch confirmation {
            case let .restore(name): Button("Restore") { run { try await restore(name) } }
            case .undo: Button("Undo Restore", role: .destructive) { run { try await undo() } }
            }
        } message: { confirmation in
            switch confirmation {
            case .restore: Text("Saves with the same names are replaced. Your current saves are kept so you can undo.")
            case .undo: Text("Puts back the saves you had before the last restore. Progress saved since then is lost.")
            }
        }
        .sheet(isPresented: $choosingGame) {
            LudusaviGamePicker(folders: service.gameFolders, current: service.match(for: game)) { folder in
                service.setMatch(folder, for: game)
                choosingGame = false
                Task { await load() }
            }
        }
    }

    @ViewBuilder private var backupList: some View {
        Section {
            if backups.isEmpty {
                Text(service.match(for: game) == nil ? "No Ludusavi game matches this one. Pick it above, or back up to start one."
                                                    : "No backups yet.")
                    .foregroundStyle(.secondary)
            }
            ForEach(backups.reversed()) { backup in
                Button { selected = backup.name } label: { BackupRow(backup: backup, isLatest: backup.name == backups.last?.name,
                                                                      isSelected: backup.name == selected) }
                    .buttonStyle(.plain)
            }
        } header: {
            Text("Backups")
        }
    }

    @ViewBuilder private var actions: some View {
        Section {
            Button("Restore Selected Backup", systemImage: "clock.arrow.circlepath") { selected.map { confirming = .restore($0) } }
                .disabled(selected == nil)
            Button("Back Up Now", systemImage: "square.and.arrow.up") { run { try await backUp() } }
            Button("Undo Last Restore", systemImage: "arrow.uturn.backward") { confirming = .undo }
                .disabled(!service.canUndoRestore(game))
        } footer: {
            if busy {
                ProgressView()
            } else if let status {
                Label(status.text, systemImage: status.isError ? "xmark.octagon.fill" : "checkmark.circle.fill")
                    .foregroundStyle(status.isError ? .red : .green)
            }
        }
    }

    private var confirmationTitle: String {
        switch confirming {
        case let .restore(name): "Restore \(backups.first { $0.name == name }.map(BackupRow.title) ?? name)?"
        case .undo: "Undo the last restore?"
        case nil: ""
        }
    }

    private func load() async {
        do {
            backups = try await service.backups(for: game)
            if selected == nil || !backups.contains(where: { $0.name == selected }) { selected = backups.last?.name }
        } catch {
            backups = []
            status = Status(text: error.localizedDescription, isError: true)
        }
    }

    private func run(_ work: @escaping () async throws -> Void) {
        busy = true
        Task {
            do {
                try await work()
            } catch {
                status = Status(text: error.localizedDescription, isError: true)
            }
            busy = false
        }
    }

    private func restore(_ name: String) async throws {
        let report = try await service.restore(game, backup: name)
        status = Status(text: "Restored \(report.restored.count) saves.", isError: false)
    }

    private func undo() async throws {
        try await service.undoRestore(game)
        status = Status(text: "Your saves from before the restore are back.", isError: false)
    }

    private func backUp() async throws {
        let report = try await service.backUp(game)
        selected = nil
        await load()
        guard let name = report.backup else {
            status = Status(text: "The latest backup already has these saves.", isError: false)
            return
        }
        var text = "Saved \(report.isDifferential ? "a differential" : "a full") backup, \(name)."
        if let count = report.paths[.vndegen] {
            text += " \(count) saves had no earlier backup to follow, so Ludusavi will restore them under /VNDegen until you add a redirect."
        }
        status = Status(text: text, isError: false)
    }
}

private struct BackupRow: View {
    let backup: BackupSummary
    let isLatest: Bool
    let isSelected: Bool

    static func title(_ backup: BackupSummary) -> String {
        backup.when?.formatted(date: .abbreviated, time: .shortened) ?? backup.name
    }

    var body: some View {
        HStack(spacing: 14) {
            Image(systemName: isSelected ? "checkmark.circle.fill" : "circle")
                .font(.title3)
                .foregroundStyle(isSelected ? AnyShapeStyle(.tint) : AnyShapeStyle(.tertiary))
            VStack(alignment: .leading, spacing: 3) {
                HStack(spacing: 8) {
                    Text(Self.title(backup)).font(.body.weight(.semibold))
                    if isLatest {
                        Text("Latest")
                            .font(.caption2.weight(.bold))
                            .padding(.horizontal, 7)
                            .padding(.vertical, 2)
                            .background(.tint, in: .capsule)
                    }
                }
                Text(detail).font(.caption).foregroundStyle(.secondary)
            }
            Spacer()
        }
        .padding(.vertical, 2)
        .contentShape(.rect)
        .accessibilityAddTraits(isSelected ? .isSelected : [])
    }

    private var detail: String {
        var parts = [osName, "\(backup.fileCount) files"]
        if backup.isDifferential { parts.append("\(backup.changedCount) changed") }
        return parts.joined(separator: " · ")
    }

    private var osName: String {
        switch backup.os {
        case "linux": "Linux"
        case "mac": "Mac"
        case "windows": "Windows"
        default: backup.comment ?? "Other"
        }
    }
}

private struct LudusaviGamePicker: View {
    let folders: [String]
    let current: String?
    let onPick: (String?) -> Void
    @State private var query = ""
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List {
                Button { onPick(nil) } label: { Label("Match automatically", systemImage: "wand.and.stars") }
                ForEach(folders.filter { query.isEmpty || $0.localizedCaseInsensitiveContains(query) }, id: \.self) { folder in
                    Button { onPick(folder) } label: {
                        HStack {
                            Text(folder)
                            Spacer()
                            if folder == current { Image(systemName: "checkmark.circle.fill").foregroundStyle(.tint) }
                        }
                        .contentShape(.rect)
                    }
                    .buttonStyle(.plain)
                }
            }
            .searchable(text: $query)
            .navigationTitle("Ludusavi Game")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Cancel") { dismiss() } }
            }
        }
        #if os(macOS)
        .frame(minWidth: 420, minHeight: 480)
        #endif
    }
}
