import SwiftUI
import UniformTypeIdentifiers
import VNCore

/// The state behind `SaveSyncSettingsView`, including gamepad focus. A host that drives the view with a controller keeps
/// the model and forwards button presses to `handle(_:)`.
@MainActor
@Observable
public final class SaveSyncSettingsModel {
    enum Target: Hashable {
        case chooseFolder, suggestedFolder, forgetFolder, matchRetention, fullBackups, differentialBackups, autoBackup
    }

    public let service: SaveSyncService
    var picking = false
    var focus = FocusRing<Target>()

    public init(service: SaveSyncService) { self.service = service }

    var rows: [Target] {
        var rows: [Target] = [.chooseFolder]
        if let suggested = service.suggestedFolder, service.folder != .ready(suggested) { rows.append(.suggestedFolder) }
        if service.folder != .none { rows.append(.forgetFolder) }
        if service.configRetention == nil {
            rows.append(.matchRetention)
            if service.manualRetention != nil { rows += [.fullBackups, .differentialBackups] }
        }
        return rows + [.autoBackup]
    }

    /// Directions move focus, confirm activates the focused control, and left and right also change a number.
    /// Returns false for buttons the host should handle, such as cancel.
    public func handle(_ button: VirtualButton) -> Bool {
        let rows = rows.map { [$0] }
        switch button {
        case .left, .right:
            let step = button == .left ? -1 : 1
            switch focus.current(in: rows) {
            case .fullBackups?: service.manualRetention?.full = min(255, max(1, (service.manualRetention?.full ?? 1) + step))
            case .differentialBackups?: service.manualRetention?.differential = min(255, max(0, (service.manualRetention?.differential ?? 0) + step))
            default: break
            }
            focus.showsFocus = true
            return true
        case .confirm:
            focus.showsFocus = true
            if let target = focus.current(in: rows) { activate(target) }
            return true
        default:
            return focus.move(button, in: rows)
        }
    }

    /// Touch and pointer: focus the control without showing the ring, then activate it.
    func select(_ target: Target) {
        focus.select(target)
        activate(target)
    }

    private func activate(_ target: Target) {
        switch target {
        case .chooseFolder: picking = true
        case .suggestedFolder: if let suggested = service.suggestedFolder { service.chooseFolder(suggested) }
        case .forgetFolder: service.forgetFolder()
        case .matchRetention: service.manualRetention = service.manualRetention == nil ? Retention(full: 1, differential: 1) : nil
        case .fullBackups: service.manualRetention?.full = min(255, (service.manualRetention?.full ?? 1) + 1)
        case .differentialBackups: service.manualRetention?.differential = min(255, (service.manualRetention?.differential ?? 0) + 1)
        case .autoBackup: service.autoBackup.toggle()
        }
    }
}

/// Ludusavi folder and automatic backup, shown from the app's Settings screen. It draws a plain grouped form and takes
/// its colors from the host's color scheme and tint.
public struct SaveSyncSettingsView: View {
    @State private var model: SaveSyncSettingsModel

    public init(service: SaveSyncService) { _model = State(initialValue: SaveSyncSettingsModel(service: service)) }
    public init(model: SaveSyncSettingsModel) { _model = State(initialValue: model) }

    private var service: SaveSyncService { model.service }

    public var body: some View {
        @Bindable var model = model
        @Bindable var service = service
        let rows = model.rows.map { [$0] }
        ScrollViewReader { proxy in
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
                    Button(service.folder == .none ? "Choose Folder…" : "Choose Another Folder…", systemImage: "folder") { model.select(.chooseFolder) }
                        .focusRow(model.focus.isFocused(.chooseFolder, in: rows), id: SaveSyncSettingsModel.Target.chooseFolder)
                    if let suggested = service.suggestedFolder, service.folder != .ready(suggested) {
                        Button { model.select(.suggestedFolder) } label: {
                            FolderRow(url: suggested, detail: "Use Ludusavi's backup folder")
                        }
                        .buttonStyle(.plain)
                        .focusRow(model.focus.isFocused(.suggestedFolder, in: rows), id: SaveSyncSettingsModel.Target.suggestedFolder)
                    }
                    if service.folder != .none {
                        // Hosts may set a plain foreground style, which would otherwise hide the destructive red.
                        Button("Forget Folder", systemImage: "xmark.circle", role: .destructive) { model.select(.forgetFolder) }
                            .foregroundStyle(.red)
                            .focusRow(model.focus.isFocused(.forgetFolder, in: rows), id: SaveSyncSettingsModel.Target.forgetFolder)
                    }
                } header: {
                    Text("Backup folder")
                } footer: {
                    Text("Pick the folder Ludusavi backs up to. For a folder on a server, connect to the server in the Files app first.")
                }

                retention(rows)

                Section {
                    Toggle("Back up when a game closes", isOn: $service.autoBackup)
                        .focusRow(model.focus.isFocused(.autoBackup, in: rows), id: SaveSyncSettingsModel.Target.autoBackup)
                } footer: {
                    Text("Adds a new Ludusavi backup each time you quit a game that has one. Existing backups are never changed.")
                }
            }
            .formStyle(.grouped)
            .onChange(of: model.focus.target) { _, target in
                guard model.focus.showsFocus, let target else { return }
                withAnimation { proxy.scrollTo(target, anchor: .center) }
            }
        }
        .fileImporter(isPresented: $model.picking, allowedContentTypes: [.folder]) { result in
            if case let .success(url) = result { service.chooseFolder(url) }
        }
        .task(id: service.folder) { await service.refresh() }
    }
}

extension SaveSyncSettingsView {
    /// Decides between full and differential backups the way Ludusavi would.
    @ViewBuilder private func retention(_ rows: [[SaveSyncSettingsModel.Target]]) -> some View {
        Section {
            if let limits = service.configRetention {
                LabeledContent("Full backups", value: "\(limits.full)")
                LabeledContent("Differential backups", value: "\(limits.differential)")
            } else {
                Toggle("Match my Ludusavi settings", isOn: Binding(
                    get: { service.manualRetention != nil },
                    set: { service.manualRetention = $0 ? Retention(full: 1, differential: 1) : nil }))
                    .focusRow(model.focus.isFocused(.matchRetention, in: rows), id: SaveSyncSettingsModel.Target.matchRetention)
                if let limits = service.manualRetention {
                    Stepper("Full backups: \(limits.full)", value: Binding(
                        get: { limits.full }, set: { service.manualRetention?.full = $0 }), in: 1...255)
                        .focusRow(model.focus.isFocused(.fullBackups, in: rows), id: SaveSyncSettingsModel.Target.fullBackups)
                    Stepper("Differential backups: \(limits.differential)", value: Binding(
                        get: { limits.differential }, set: { service.manualRetention?.differential = $0 }), in: 0...255)
                        .focusRow(model.focus.isFocused(.differentialBackups, in: rows), id: SaveSyncSettingsModel.Target.differentialBackups)
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

/// The state behind `GameSavesView`, including gamepad focus. A host that drives the view with a controller keeps the
/// model and forwards button presses to `handle(_:)`.
@MainActor
@Observable
public final class GameSavesModel {
    enum Target: Hashable {
        case ludusaviGame, backup(String), restore, backUp, undo
        /// Rows of the Ludusavi game picker.
        case automaticMatch, folder(String)
    }

    struct Status {
        var text: String
        var isError: Bool
    }

    enum Confirmation: Identifiable {
        case restore(String), undo
        var id: String { if case let .restore(name) = self { name } else { "undo" } }
    }

    public let service: SaveSyncService
    public let game: GameID
    var backups: [BackupSummary] = []
    var selected: String?
    var busy = false
    var status: Status?
    var confirming: Confirmation?
    var choosingGame = false
    var query = ""
    var focus = FocusRing<Target>()

    public init(service: SaveSyncService, game: GameID) {
        self.service = service
        self.game = game
    }

    var filteredFolders: [String] {
        service.gameFolders.filter { query.isEmpty || $0.localizedCaseInsensitiveContains(query) }
    }

    var rows: [[Target]] {
        if choosingGame { return [[.automaticMatch]] + filteredFolders.map { [.folder($0)] } }
        guard case .ready = service.folder else { return [] }
        return [[.ludusaviGame]] + backups.reversed().map { [.backup($0.name)] } + [[.restore], [.backUp], [.undo]]
    }

    private var defaultTarget: Target? {
        if choosingGame { return service.match(for: game).map(Target.folder) ?? .automaticMatch }
        return selected.map(Target.backup) ?? rows.first?.first
    }

    func isFocused(_ target: Target) -> Bool { focus.isFocused(target, in: rows, default: defaultTarget) }

    /// Directions move focus and confirm activates the focused control. While a confirmation is up, confirm accepts it
    /// and cancel dismisses it. Returns false for buttons the host should handle, such as cancel outside a dialog.
    public func handle(_ button: VirtualButton) -> Bool {
        if let confirming {
            switch button {
            case .confirm: perform(confirming)
            case .cancel: self.confirming = nil
            default: break
            }
            return true
        }
        switch button {
        case .cancel:
            guard choosingGame else { return false }
            closePicker()
            return true
        case .confirm:
            focus.showsFocus = true
            if !busy, let target = focus.current(in: rows, default: defaultTarget) { activate(target) }
            return true
        default:
            return focus.move(button, in: rows, default: defaultTarget)
        }
    }

    /// Touch and pointer: focus the control without showing the ring, then activate it.
    func select(_ target: Target) {
        focus.select(target)
        activate(target)
    }

    private func activate(_ target: Target) {
        switch target {
        case .ludusaviGame:
            query = ""
            choosingGame = true
        case let .backup(name): selected = name
        case .restore: if let selected { confirming = .restore(selected) }
        case .backUp: run { try await self.backUp() }
        case .undo: if service.canUndoRestore(game) { confirming = .undo }
        case .automaticMatch: pick(nil)
        case let .folder(folder): pick(folder)
        }
    }

    func perform(_ confirmation: Confirmation) {
        confirming = nil
        switch confirmation {
        case let .restore(name): run { try await self.restore(name) }
        case .undo: run { try await self.undo() }
        }
    }

    private func pick(_ folder: String?) {
        service.setMatch(folder, for: game)
        closePicker()
        Task { await load() }
    }

    /// Focus returns to the row that opened the picker.
    func closePicker() {
        choosingGame = false
        focus.target = .ludusaviGame
    }

    func load() async {
        do {
            backups = try await service.backups(for: game)
            if selected == nil || !backups.contains(where: { $0.name == selected }) { selected = backups.last?.name }
        } catch {
            backups = []
            status = Status(text: error.localizedDescription, isError: true)
        }
    }

    private func run(_ work: @escaping () async throws -> Void) {
        guard !busy else { return }
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

/// A game's Ludusavi backups, with restore, back up, and undo. Shown from the game page, styled like
/// `SaveSyncSettingsView`.
public struct GameSavesView: View {
    @State private var model: GameSavesModel

    public init(service: SaveSyncService, game: GameID) { _model = State(initialValue: GameSavesModel(service: service, game: game)) }
    public init(model: GameSavesModel) { _model = State(initialValue: model) }

    public var body: some View {
        @Bindable var model = model
        ScrollViewReader { proxy in
            Form {
                if case .ready = model.service.folder {
                    Section("Ludusavi game") {
                        Button { model.select(.ludusaviGame) } label: {
                            HStack {
                                Text("Backups from")
                                Spacer()
                                Text(model.service.match(for: model.game) ?? "No match").foregroundStyle(.secondary).lineLimit(1)
                                Image(systemName: "chevron.right").font(.footnote.weight(.semibold)).foregroundStyle(.tertiary)
                            }
                            .contentShape(.rect)
                        }
                        .buttonStyle(.plain)
                        .focusRow(model.isFocused(.ludusaviGame), id: GameSavesModel.Target.ludusaviGame)
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
            .disabled(model.busy)
            .onChange(of: model.focus.target) { _, target in
                guard model.focus.showsFocus, let target else { return }
                withAnimation { proxy.scrollTo(target, anchor: .center) }
            }
        }
        .task(id: model.service.folder) {
            await model.service.refresh()
            await model.load()
        }
        .confirmationDialog(confirmationTitle, isPresented: .init(get: { model.confirming != nil }, set: { if !$0 { model.confirming = nil } }),
                            titleVisibility: .visible, presenting: model.confirming) { confirmation in
            switch confirmation {
            case .restore: Button("Restore") { model.perform(confirmation) }
            case .undo: Button("Undo Restore", role: .destructive) { model.perform(confirmation) }
            }
        } message: { confirmation in
            // The dialog's buttons show no gamepad focus, so controller users get the buttons spelled out.
            let hint = model.focus.showsFocus ? " Press A to confirm or B to cancel." : ""
            switch confirmation {
            case .restore: Text("Saves with the same names are replaced. Your current saves are kept so you can undo." + hint)
            case .undo: Text("Puts back the saves you had before the last restore. Progress saved since then is lost." + hint)
            }
        }
        .sheet(isPresented: $model.choosingGame) {
            LudusaviGamePicker(model: model)
        }
    }

    @ViewBuilder private var backupList: some View {
        Section {
            if model.backups.isEmpty {
                Text(model.service.match(for: model.game) == nil ? "No Ludusavi game matches this one. Pick it above, or back up to start one."
                                                                : "No backups yet.")
                    .foregroundStyle(.secondary)
            }
            ForEach(model.backups.reversed()) { backup in
                let target = GameSavesModel.Target.backup(backup.name)
                Button { model.select(target) } label: {
                    BackupRow(backup: backup, isLatest: backup.name == model.backups.last?.name, isSelected: backup.name == model.selected)
                }
                .buttonStyle(.plain)
                .focusRow(model.isFocused(target), id: target)
            }
        } header: {
            Text("Backups")
        }
    }

    @ViewBuilder private var actions: some View {
        Section {
            Button("Restore Selected Backup", systemImage: "clock.arrow.circlepath") { model.select(.restore) }
                .disabled(model.selected == nil)
                .focusRow(model.isFocused(.restore), id: GameSavesModel.Target.restore)
            Button("Back Up Now", systemImage: "square.and.arrow.up") { model.select(.backUp) }
                .focusRow(model.isFocused(.backUp), id: GameSavesModel.Target.backUp)
            Button("Undo Last Restore", systemImage: "arrow.uturn.backward") { model.select(.undo) }
                .disabled(!model.service.canUndoRestore(model.game))
                .focusRow(model.isFocused(.undo), id: GameSavesModel.Target.undo)
        } footer: {
            if model.busy {
                ProgressView()
            } else if let status = model.status {
                Label(status.text, systemImage: status.isError ? "xmark.octagon.fill" : "checkmark.circle.fill")
                    .foregroundStyle(status.isError ? .red : .green)
            }
        }
    }

    private var confirmationTitle: String {
        switch model.confirming {
        case let .restore(name): "Restore \(model.backups.first { $0.name == name }.map(BackupRow.title) ?? name)?"
        case .undo: "Undo the last restore?"
        case nil: ""
        }
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
    @Bindable var model: GameSavesModel

    var body: some View {
        let current = model.service.match(for: model.game)
        NavigationStack {
            ScrollViewReader { proxy in
                List {
                    Button { model.select(.automaticMatch) } label: { Label("Match automatically", systemImage: "wand.and.stars") }
                        .focusRow(model.isFocused(.automaticMatch), id: GameSavesModel.Target.automaticMatch)
                    ForEach(model.filteredFolders, id: \.self) { folder in
                        let target = GameSavesModel.Target.folder(folder)
                        Button { model.select(target) } label: {
                            HStack {
                                Text(folder)
                                Spacer()
                                if folder == current { Image(systemName: "checkmark.circle.fill").foregroundStyle(.tint) }
                            }
                            .contentShape(.rect)
                        }
                        .buttonStyle(.plain)
                        .focusRow(model.isFocused(target), id: target)
                    }
                }
                .onChange(of: model.focus.target) { _, target in
                    guard model.focus.showsFocus, let target else { return }
                    withAnimation { proxy.scrollTo(target, anchor: .center) }
                }
            }
            .searchable(text: $model.query)
            .navigationTitle("Ludusavi Game")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Cancel") { model.closePicker() } }
            }
        }
        #if os(macOS)
        .frame(minWidth: 420, minHeight: 480)
        #endif
    }
}

// MARK: Gamepad focus

/// Which control has gamepad focus, among rows of controls that the owning model derives from its state.
struct FocusRing<Target: Hashable> {
    var target: Target?
    /// The ring shows only once a controller moved it, so touch users never see it.
    var showsFocus = false

    /// The focused control, or the default when the stored one is no longer on screen.
    func current(in rows: [[Target]], default fallback: Target? = nil) -> Target? {
        if let target, rows.contains(where: { $0.contains(target) }) { return target }
        return fallback ?? rows.first?.first
    }

    func isFocused(_ candidate: Target, in rows: [[Target]], default fallback: Target? = nil) -> Bool {
        showsFocus && current(in: rows, default: fallback) == candidate
    }

    /// Up and down change rows, left and right move within one. Returns false for buttons that are not directions.
    mutating func move(_ button: VirtualButton, in rows: [[Target]], default fallback: Target? = nil) -> Bool {
        guard [.up, .down, .left, .right].contains(button) else { return false }
        defer { showsFocus = true }
        guard let current = current(in: rows, default: fallback),
              let r = rows.firstIndex(where: { $0.contains(current) }), let c = rows[r].firstIndex(of: current) else { return true }
        // The first press only reveals the ring where focus already is.
        guard showsFocus else { return true }
        switch button {
        case .up where r > 0: target = rows[r - 1][min(c, rows[r - 1].count - 1)]
        case .down where r < rows.count - 1: target = rows[r + 1][min(c, rows[r + 1].count - 1)]
        case .left: target = rows[r][max(0, c - 1)]
        case .right: target = rows[r][min(rows[r].count - 1, c + 1)]
        default: target = current
        }
        return true
    }

    mutating func select(_ candidate: Target) {
        target = candidate
        showsFocus = false
    }
}

/// The row background for gamepad focus: a tinted fill with a white outline, like the hub's focus ring.
private struct FocusRowBackground: View {
    var body: some View {
        RoundedRectangle(cornerRadius: 10, style: .continuous)
            .fill(.tint.opacity(0.32))
            .strokeBorder(.white, lineWidth: 2.5)
    }
}

private extension View {
    func focusRow(_ focused: Bool, id: some Hashable) -> some View {
        listRowBackground(focused ? FocusRowBackground() : nil)
            .animation(.spring(response: 0.28, dampingFraction: 0.78), value: focused)
            .id(id)
    }
}
