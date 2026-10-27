import SwiftUI
import VNCore
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

/// The state behind `SaveSyncSettingsView`, including gamepad focus. A host that drives the view with a controller keeps
/// the model and forwards button presses to `handle(_:)`. The text fields take touch and keyboard only.
@MainActor
@Observable
public final class SaveSyncSettingsModel {
    enum Target: Hashable {
        case guest, test, forget, matchRetention, fullBackups, differentialBackups, autoBackup, checkBeforeLaunch
    }

    public let service: SaveSyncService
    var host: String
    var port: String
    /// Ludusavi's `guest` login, with no password. On for a new setup.
    var guest: Bool
    var username: String
    var password: String
    var path: String
    /// Why the details could not be saved, such as a Keychain error.
    var problem: String?
    var focus = FocusRing<Target>()

    public init(service: SaveSyncService) {
        self.service = service
        let smb: SMBRemote? = switch service.server?.remote {
        case let .smb(smb)?: smb
        case nil: nil
        }
        host = smb?.host ?? ""
        port = String(smb?.port ?? SMBRemote.defaultPort)
        guest = smb?.isGuest ?? true
        username = smb.flatMap { $0.isGuest ? nil : $0.username } ?? ""
        password = service.storedPassword() ?? ""
        path = service.server?.path ?? ""
    }

    var canTest: Bool { !host.isEmpty && !path.isEmpty && Int(port) != nil && (guest || !username.isEmpty) }

    var rows: [Target] {
        var rows: [Target] = [.guest, .test]
        if service.server != nil { rows.append(.forget) }
        if service.configRetention == nil {
            rows.append(.matchRetention)
            if service.manualRetention != nil { rows += [.fullBackups, .differentialBackups] }
        }
        return rows + [.autoBackup, .checkBeforeLaunch]
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
        case .guest: guest.toggle()
        case .test:
            guard canTest, service.connection != .checking else { return }
            do {
                try service.setServer(host: host, port: Int(port) ?? SMBRemote.defaultPort, guest: guest, username: username, password: password,
                                      path: path)
                problem = nil
            } catch {
                problem = error.localizedDescription
                return
            }
            Task { await service.testConnection() }
        case .forget:
            service.forgetServer()
            host = ""; port = String(SMBRemote.defaultPort); guest = true; username = ""; password = ""; path = ""
            problem = nil
        case .matchRetention: service.manualRetention = service.manualRetention == nil ? Retention(full: 1, differential: 1) : nil
        case .fullBackups: service.manualRetention?.full = min(255, (service.manualRetention?.full ?? 1) + 1)
        case .differentialBackups: service.manualRetention?.differential = min(255, (service.manualRetention?.differential ?? 0) + 1)
        case .autoBackup: service.autoBackup.toggle()
        case .checkBeforeLaunch: service.checkBeforeLaunch.toggle()
        }
    }
}

/// What save sync says about settings from VNDegen 0.4.0, which reached the store over SSH.
let oldServerNote = "Save sync now connects to your Ludusavi share over SMB, the way Ludusavi on your PC does. Enter the share in Settings to turn it back on."

/// The Ludusavi share, retention, and automatic sync, shown from the app's Settings screen. It draws a plain grouped form
/// and takes its colors from the host's color scheme and tint.
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
                if service.needsShare {
                    Section {
                        Label(oldServerNote, systemImage: "info.circle.fill").foregroundStyle(.orange)
                    }
                }
                Section {
                    LabeledContent("Host") { field("nas.example", text: $model.host) }
                    LabeledContent("Port") { field(String(SMBRemote.defaultPort), text: $model.port) }
                    Toggle("Guest", isOn: Binding(get: { model.guest }, set: { _ in model.select(.guest) }))
                        .focusRow(model.focus.isFocused(.guest, in: rows), id: SaveSyncSettingsModel.Target.guest)
                    if !model.guest {
                        LabeledContent("Username") { field("username", text: $model.username) }
                        LabeledContent("Password") {
                            SecureField("Password", text: $model.password, prompt: Text("password"))
                                .labelsHidden()
                                .multilineTextAlignment(.trailing)
                        }
                    }
                    LabeledContent("Path") { field("games/Ludusavi", text: $model.path) }
                    connectionStatus
                    Button(service.connection == .checking ? "Connecting…" : "Test Connection", systemImage: "network") { model.select(.test) }
                        .disabled(!model.canTest || service.connection == .checking)
                        .focusRow(model.focus.isFocused(.test, in: rows), id: SaveSyncSettingsModel.Target.test)
                    if service.server != nil {
                        // Hosts may set a plain foreground style, which would otherwise hide the destructive red.
                        Button("Forget Server", systemImage: "xmark.circle", role: .destructive) { model.select(.forget) }
                            .foregroundStyle(.red)
                            .focusRow(model.focus.isFocused(.forget, in: rows), id: SaveSyncSettingsModel.Target.forget)
                    }
                } header: {
                    Text("Ludusavi share")
                } footer: {
                    Text("Enter what Ludusavi's SMB cloud settings show. The path is the share, then the folder inside it, such as games/Ludusavi. VNDegen copies new backups to the share and never syncs up to it, so it deletes nothing another device wrote. The password stays on this device.")
                }

                retention(rows)

                Section {
                    Toggle("Back up when a game closes", isOn: $service.autoBackup)
                        .focusRow(model.focus.isFocused(.autoBackup, in: rows), id: SaveSyncSettingsModel.Target.autoBackup)
                    Toggle("Check for newer saves before playing", isOn: $service.checkBeforeLaunch)
                        .focusRow(model.focus.isFocused(.checkBeforeLaunch, in: rows), id: SaveSyncSettingsModel.Target.checkBeforeLaunch)
                } footer: {
                    Text("A backup happens only when your saves changed. When a newer backup came from another device, VNDegen asks before the game starts.")
                }
            }
            .formStyle(.grouped)
            .onChange(of: model.focus.target) { _, target in
                guard model.focus.showsFocus, let target else { return }
                withAnimation { proxy.scrollTo(target, anchor: .center) }
            }
        }
        .task { await service.refresh() }
    }

    private func field(_ prompt: String, text: Binding<String>) -> some View {
        // The row's label names the field. macOS would show a TextField's own title as a second label.
        TextField(prompt, text: text, prompt: Text(prompt))
            .labelsHidden()
            .multilineTextAlignment(.trailing)
            .autocorrectionDisabled()
            #if os(iOS)
            .textInputAutocapitalization(.never)
            #endif
    }

    @ViewBuilder private var connectionStatus: some View {
        if let problem = model.problem {
            Label(problem, systemImage: "exclamationmark.triangle.fill").foregroundStyle(.orange)
        } else {
            switch service.connection {
            case .notSetUp:
                Text(service.server == nil ? "Not set up" : "Not tested yet").foregroundStyle(.secondary)
            case .checking:
                ProgressView()
            case .connected:
                let count = service.foundGames ?? service.storeGames.count
                Label(count == 1 ? "Connected. Found 1 game." : "Connected. Found \(count) games.", systemImage: "checkmark.circle.fill")
                    .foregroundStyle(.green)
            case let .failed(message):
                Label(message, systemImage: "exclamationmark.triangle.fill").foregroundStyle(.orange)
            }
        }
    }

    /// Decides between full and differential backups, and how many to keep, the way Ludusavi would.
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
                 ? "From Ludusavi's settings on this Mac. Backups beyond these limits leave the backup list, and Ludusavi on a PC deletes their folders at its next backup."
                 : "Enter the limits from Ludusavi's Other screen. VNDegen then makes full or differential backups and drops old ones from the backup list as Ludusavi would. Ludusavi on a PC deletes their folders at its next backup. When off, VNDegen follows the backups already on the share and drops none.")
        }
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
        case restore(String), undo, backUp
        var id: String {
            switch self {
            case let .restore(name): name
            case .undo: "undo"
            case .backUp: "backUp"
            }
        }
    }

    public let service: SaveSyncService
    public let game: GameID
    var backups: [BackupSummary] = []
    var selected: String? {
        didSet { if selected != oldValue { Task { await loadPreview() } } }
    }
    var preview: RestorePreview?
    var sync: SyncStatus?
    /// The server state `sync` describes. Back Up runs only on top of it, because the user decided on this one, so it
    /// stays off until a check succeeds.
    private(set) var judged: ServerRevision?
    /// True until the first load finishes, so the list says it is loading rather than empty.
    var loading = true
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

    /// Store folders and their titles, filtered by the search text.
    var filteredFolders: [(folder: String, title: String)] {
        service.storeGames.sorted { $0.value.localizedStandardCompare($1.value) == .orderedAscending }
            .filter { query.isEmpty || $0.value.localizedCaseInsensitiveContains(query) || $0.key.localizedCaseInsensitiveContains(query) }
            .map { ($0.key, $0.value) }
    }

    private var isSetUp: Bool { service.server != nil }

    var rows: [[Target]] {
        if choosingGame { return [[.automaticMatch]] + filteredFolders.map { [.folder($0.folder)] } }
        guard isSetUp else { return [] }
        return [[.ludusaviGame]] + backups.reversed().map { [.backup($0.name)] } + [[.restore], [.backUp], [.undo]]
    }

    private var defaultTarget: Target? {
        if choosingGame { return service.match(for: game).map { Target.folder($0.folder) } ?? .automaticMatch }
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
        case .backUp:
            guard let judged else { break }
            if service.backupWarning(game) == nil { run { try await self.backUp(on: judged) } } else { confirming = .backUp }
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
        case .backUp: if let judged { run { try await self.backUp(on: judged) } }
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
        loading = true
        guard isSetUp else { loading = false; return }
        defer { loading = false }
        do {
            backups = try await service.backups(for: game)
            if selected == nil || !backups.contains(where: { $0.name == selected }) { selected = backups.last?.name }
            let check = try await service.check(game)
            (sync, judged) = (check.status, check.revision)
            await loadPreview()
        } catch {
            backups = []
            judged = nil
            status = Status(text: error.localizedDescription, isError: true)
        }
    }

    private func loadPreview() async {
        guard let selected else { preview = nil; return }
        preview = try? await service.preview(game, backup: selected)
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
        await refreshSync()
        await loadPreview()
    }

    private func undo() async throws {
        try await service.undoRestore(game)
        status = Status(text: "Your saves from before the restore are back.", isError: false)
        await refreshSync()
        await loadPreview()
    }

    private func refreshSync() async {
        let check = try? await service.check(game)
        (sync, judged) = (check?.status, check?.revision)
    }

    private func backUp(on judged: ServerRevision) async throws {
        let report: BackupReport
        do {
            report = try await service.backUp(game, judged: judged)
        } catch SaveSyncError.changedSinceCheck {
            await load()
            throw SaveSyncError.changedSinceCheck
        }
        selected = nil
        await load()
        guard let name = report.backup else {
            status = Status(text: "The latest backup already has these saves.", isError: false)
            return
        }
        var text = "Saved \(report.isDifferential ? "a differential" : "a full") backup, \(name)."
        if let count = report.paths[.vndegen] {
            text += " \(count) saves had no PC folder to follow, so Ludusavi will restore them under /VNDegen until you add a redirect."
        }
        if !report.pruned.isEmpty {
            text += " Dropped \(report.pruned.count) old backups from the list, as your retention limits ask. Ludusavi deletes their folders at its next backup of this game."
        }
        status = Status(text: text, isError: false)
    }
}

/// A game's Ludusavi backups, with sync state, restore with a preview of what changes, back up, and undo. Shown from
/// the game page, styled like `SaveSyncSettingsView`.
public struct GameSavesView: View {
    @State private var model: GameSavesModel

    public init(service: SaveSyncService, game: GameID) { _model = State(initialValue: GameSavesModel(service: service, game: game)) }
    public init(model: GameSavesModel) { _model = State(initialValue: model) }

    public var body: some View {
        @Bindable var model = model
        ScrollViewReader { proxy in
            Form {
                if model.service.server != nil {
                    syncSection
                    matchSection
                    backupList
                    actions
                    locations
                } else {
                    Section {
                        Text(model.service.needsShare ? oldServerNote : "Set up your Ludusavi share in Settings to restore and back up this game's saves.")
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
        .task {
            await model.service.refresh()
            await model.load()
        }
        .confirmationDialog(confirmationTitle, isPresented: .init(get: { model.confirming != nil }, set: { if !$0 { model.confirming = nil } }),
                            titleVisibility: .visible, presenting: model.confirming) { confirmation in
            switch confirmation {
            case .restore: Button("Restore") { model.perform(confirmation) }
            case .undo: Button("Undo Restore", role: .destructive) { model.perform(confirmation) }
            case .backUp: Button("Back Up") { model.perform(confirmation) }
            }
        } message: { confirmation in
            // The dialog's buttons show no gamepad focus, so controller users get the buttons spelled out.
            let hint = model.focus.showsFocus ? " Press A to confirm or B to cancel." : ""
            switch confirmation {
            case .restore: Text("Saves with the same names are replaced. Your current saves are kept so you can undo." + hint)
            case .undo: Text("Puts back the saves you had before the last restore. Progress saved since then is lost." + hint)
            case .backUp: Text((model.service.backupWarning(model.game) ?? "") + hint)
            }
        }
        .sheet(isPresented: $model.choosingGame) {
            LudusaviGamePicker(model: model)
        }
    }

    @ViewBuilder private var syncSection: some View {
        if let sync = model.sync {
            Section {
                let (text, icon, color): (String, String, Color) = switch sync {
                case .noBackups: ("The server has no backup of this game yet.", "icloud.slash", .secondary)
                case .inSync: ("Your saves here match the latest backup.", "checkmark.circle.fill", .green)
                case .localChanged: ("Your saves here changed since the last backup.", "arrow.up.circle.fill", .blue)
                case let .newerBackup(backup): ("\(BackupRow.source(backup)) backed up newer saves on \(BackupRow.title(backup)).", "arrow.down.circle.fill", .blue)
                case let .conflict(backup):
                    ("Your saves here changed, and \(BackupRow.source(backup)) backed up on \(BackupRow.title(backup)). Restore that backup, or back up yours on top of it.",
                     "exclamationmark.triangle.fill", .orange)
                }
                Label(text, systemImage: icon).foregroundStyle(color)
            }
        } else if model.loading {
            Section { ProgressView() }
        }
    }

    private var matchSection: some View {
        Section("Ludusavi game") {
            Button { model.select(.ludusaviGame) } label: {
                HStack {
                    VStack(alignment: .leading, spacing: 2) {
                        Text(model.service.match(for: model.game)?.title ?? "No match").lineLimit(1)
                        Text(matchDetail).font(.caption).foregroundStyle(.secondary)
                    }
                    Spacer()
                    Image(systemName: "chevron.right").font(.footnote.weight(.semibold)).foregroundStyle(.tertiary)
                }
                .contentShape(.rect)
            }
            .buttonStyle(.plain)
            .focusRow(model.isFocused(.ludusaviGame), id: GameSavesModel.Target.ludusaviGame)
        }
    }

    private var matchDetail: String {
        guard let match = model.service.match(for: model.game) else { return "" }
        let how = switch match.source {
        case .picked: "Picked by you"
        case let .steamID(id): "Matched by Steam app \(id)"
        case .storeTitle: "Matched by title"
        case .manifestTitle: "Matched by title in Ludusavi's manifest"
        case .gameTitle: "No match. A first backup uses the game's title"
        }
        return match.inStore ? how : how + ". Not on the server yet"
    }

    @ViewBuilder private var backupList: some View {
        Section {
            if model.backups.isEmpty {
                Text(model.loading ? "Loading…" : "No backups yet.").foregroundStyle(.secondary)
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
        } footer: {
            if let preview = model.preview { Text(Self.describe(preview)) }
        }
    }

    /// "Restoring this backup adds 2 saves and replaces 3. 18 already match. 1 save here is not in it and stays."
    static func describe(_ preview: RestorePreview) -> String {
        var parts: [String] = []
        if !preview.added.isEmpty { parts.append("adds \(count(preview.added.count))") }
        if !preview.replaced.isEmpty { parts.append("replaces \(preview.added.isEmpty ? count(preview.replaced.count) : String(preview.replaced.count))") }
        var text = parts.isEmpty ? "Restoring this backup changes nothing." : "Restoring this backup \(parts.joined(separator: " and "))."
        if !preview.unchanged.isEmpty, !parts.isEmpty { text += " \(preview.unchanged.count) already match." }
        if !preview.kept.isEmpty {
            text += preview.kept.count == 1 ? " 1 save here is not in it and stays." : " \(preview.kept.count) saves here are not in it and stay."
        }
        return text
    }

    private static func count(_ n: Int) -> String { n == 1 ? "1 save" : "\(n) saves" }

    @ViewBuilder private var actions: some View {
        Section {
            Button("Restore Selected Backup", systemImage: "clock.arrow.circlepath") { model.select(.restore) }
                .disabled(model.selected == nil)
                .focusRow(model.isFocused(.restore), id: GameSavesModel.Target.restore)
            Button("Back Up Now", systemImage: "square.and.arrow.up") { model.select(.backUp) }
                .disabled(model.judged == nil)
                .focusRow(model.isFocused(.backUp), id: GameSavesModel.Target.backUp)
            Button("Undo Last Restore", systemImage: "arrow.uturn.backward") { model.select(.undo) }
                .disabled(!model.service.canUndoRestore(model.game))
                .focusRow(model.isFocused(.undo), id: GameSavesModel.Target.undo)
        } footer: {
            if let warning = model.service.backupWarning(model.game) {
                Label(warning, systemImage: "exclamationmark.triangle.fill").foregroundStyle(.yellow)
            }
            if model.busy {
                ProgressView()
            } else if let status = model.status {
                Label(status.text, systemImage: status.isError ? "xmark.octagon.fill" : "checkmark.circle.fill")
                    .foregroundStyle(status.isError ? .red : .green)
            }
        }
    }

    @ViewBuilder private var locations: some View {
        let places = model.service.locations(for: model.game)
        let rules = model.service.manifestRules(for: model.game)
        if !places.isEmpty || !rules.isEmpty {
            Section("Save locations") {
                ForEach(Array(places.enumerated()), id: \.offset) { _, place in
                    PathRow(title: "On this device", path: place.device)
                    if let recorded = place.recorded { PathRow(title: "In the backups", path: recorded) }
                }
                ForEach(rules, id: \.self) { PathRow(title: "Ludusavi's manifest", path: $0) }
            }
        }
    }

    private var confirmationTitle: String {
        switch model.confirming {
        case let .restore(name): "Restore \(model.backups.first { $0.name == name }.map(BackupRow.title) ?? name)?"
        case .undo: "Undo the last restore?"
        case .backUp: "Back up anyway?"
        case nil: ""
        }
    }
}

private struct PathRow: View {
    let title: String
    let path: String

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(title).font(.caption).foregroundStyle(.secondary)
            Text(path).font(.caption.monospaced()).lineLimit(2).truncationMode(.middle).textSelection(.enabled)
        }
    }
}

struct BackupRow: View {
    let backup: BackupSummary
    let isLatest: Bool
    let isSelected: Bool

    static func title(_ backup: BackupSummary) -> String {
        backup.when?.formatted(date: .abbreviated, time: .shortened) ?? backup.name
    }

    /// The machine that made the backup: its OS, or VNDegen's device note for iPhone and iPad.
    static func source(_ backup: BackupSummary) -> String {
        switch backup.os {
        case "linux": "Linux"
        case "mac": backup.comment?.hasPrefix("VNDegen") == true ? backup.comment! : "Mac"
        case "windows": "Windows"
        default: backup.comment ?? "Another device"
        }
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
        var parts = [Self.source(backup), backup.isDifferential ? "Differential" : "Full", "\(backup.fileCount) files"]
        if backup.isDifferential { parts.append("\(backup.changedCount) changed") }
        return parts.joined(separator: " · ")
    }
}

private struct LudusaviGamePicker: View {
    @Bindable var model: GameSavesModel

    var body: some View {
        let current = model.service.match(for: model.game)?.folder
        NavigationStack {
            ScrollViewReader { proxy in
                List {
                    Button { model.select(.automaticMatch) } label: { Label("Match automatically", systemImage: "wand.and.stars") }
                        .focusRow(model.isFocused(.automaticMatch), id: GameSavesModel.Target.automaticMatch)
                    ForEach(model.filteredFolders, id: \.folder) { item in
                        let target = GameSavesModel.Target.folder(item.folder)
                        Button { model.select(target) } label: {
                            HStack {
                                Text(item.title)
                                Spacer()
                                if item.folder == current { Image(systemName: "checkmark.circle.fill").foregroundStyle(.tint) }
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

// MARK: Before a game starts

/// The question asked before a game starts when the server has newer saves, or both sides changed. The host shows
/// `LaunchSavesView` and forwards controller buttons to `handle(_:)`.
@MainActor
@Observable
public final class LaunchSavesModel {
    enum Target: Hashable { case restore, keep, cancel }

    public let service: SaveSyncService
    public let game: GameID
    public let offer: SyncStatus
    /// Called once with true to start the game, or false to stay in the library.
    private let finish: (Bool) -> Void
    var busy = false
    var error: String?
    var focus = FocusRing<Target>()

    public init(service: SaveSyncService, game: GameID, offer: SyncStatus, finish: @escaping (Bool) -> Void) {
        self.service = service
        self.game = game
        self.offer = offer
        self.finish = finish
        focus.target = .restore
    }

    var backup: BackupSummary? {
        switch offer {
        case let .newerBackup(backup), let .conflict(backup): backup
        default: nil
        }
    }

    private var rows: [[Target]] { [[.restore], [.keep], [.cancel]] }

    func isFocused(_ target: Target) -> Bool { focus.isFocused(target, in: rows) }

    /// Returns false for buttons the host should handle.
    public func handle(_ button: VirtualButton) -> Bool {
        switch button {
        case .confirm:
            focus.showsFocus = true
            if let target = focus.current(in: rows) { activate(target) }
            return true
        case .cancel:
            activate(.cancel)
            return true
        default:
            return focus.move(button, in: rows)
        }
    }

    func select(_ target: Target) {
        focus.select(target)
        activate(target)
    }

    private func activate(_ target: Target) {
        guard !busy else { return }
        switch target {
        case .restore:
            busy = true
            Task {
                do {
                    _ = try await service.restore(game, backup: nil)
                    finish(true)
                } catch {
                    self.error = error.localizedDescription
                }
                busy = false
            }
        case .keep:
            if let backup { service.keepLocalSaves(game, over: backup) }
            finish(true)
        case .cancel:
            finish(false)
        }
    }
}

public struct LaunchSavesView: View {
    let model: LaunchSavesModel

    public init(model: LaunchSavesModel) { self.model = model }

    public var body: some View {
        Form {
            Section {
                Label(message, systemImage: isConflict ? "exclamationmark.triangle.fill" : "arrow.down.circle.fill")
                    .foregroundStyle(isConflict ? .orange : .primary)
                if let error = model.error { Label(error, systemImage: "xmark.octagon.fill").foregroundStyle(.red) }
            }
            Section {
                Button(isConflict ? "Use the Backup and Play" : "Restore and Play", systemImage: "clock.arrow.circlepath") { model.select(.restore) }
                    .focusRow(model.isFocused(.restore), id: LaunchSavesModel.Target.restore)
                Button(isConflict ? "Keep These Saves and Play" : "Play Without Restoring", systemImage: "play.fill") { model.select(.keep) }
                    .focusRow(model.isFocused(.keep), id: LaunchSavesModel.Target.keep)
                Button("Cancel", systemImage: "xmark") { model.select(.cancel) }
                    .focusRow(model.isFocused(.cancel), id: LaunchSavesModel.Target.cancel)
            } footer: {
                if model.busy { ProgressView() } else if isConflict {
                    Text("Using the backup keeps your current saves, so Undo Last Restore on the game's Saves page brings them back.")
                }
            }
        }
        .formStyle(.grouped)
        .disabled(model.busy)
    }

    private var isConflict: Bool { if case .conflict = model.offer { true } else { false } }

    private var message: String {
        guard let backup = model.backup else { return "" }
        let when = BackupRow.title(backup), source = BackupRow.source(backup)
        return isConflict
            ? "Your saves here changed since the last sync, and \(source) backed up this game on \(when)."
            : "\(source) backed up newer saves on \(when). Your saves here have not changed since the last sync."
    }
}

// MARK: Gamepad focus

/// Which control has gamepad focus, among rows of controls that the owning model derives from its state.
struct FocusRing<Target: Hashable> {
    var target: Target?
    /// The ring shows only once a controller moved it, so touch users never see it.
    var showsFocus = false

    /// The focused control, or the default when the stored one is no longer on screen. A default that is not on screen
    /// either, such as a row the search filter hid, falls back to the first control.
    func current(in rows: [[Target]], default fallback: Target? = nil) -> Target? {
        for candidate in [target, fallback] {
            if let candidate, rows.contains(where: { $0.contains(candidate) }) { return candidate }
        }
        return rows.first?.first
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
