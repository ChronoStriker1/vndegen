import SwiftUI
import UniformTypeIdentifiers

/// The state behind `ArchiveImportView`, including gamepad focus. A host that drives the view with a controller keeps the
/// model it passed to the view and forwards button presses to `handle(_:)`.
@Observable
public final class ArchiveImportModel {
    /// The controller buttons the import flow uses. This module does not depend on VNCore's input types.
    public enum Button { case up, down, left, right, confirm, cancel }

    enum Control: Hashable {
        case chooseArchive, folderName, deleteToggle, chooseAnother, startImport, passwordField, cancelPassword, unlock, cancelImport
        case keepNested, extractNested, importAnother, done
    }

    let gamesRoot: URL
    private(set) var importer: ArchiveImporter?
    var picking = false
    var password = ""
    /// The text field with keyboard focus. The controller sets it to bring up the system keyboard.
    var editing: Control?
    private var focus: Control?
    /// The ring shows only once a controller moved it, so touch users never see it.
    private(set) var showsFocus = false
    @ObservationIgnored private var onFinish: ((String?) -> Void)?

    public init(gamesRoot: URL) { self.gamesRoot = gamesRoot }

    /// Creates the importer once, however often the view appears.
    func start(initialSelection: [URL], onFinish: @escaping (String?) -> Void) async {
        self.onFinish = onFinish
        guard importer == nil else { return }
        let importer = ArchiveImporter(gamesRoot: gamesRoot)
        self.importer = importer
        if !initialSelection.isEmpty { await importer.choose(initialSelection) }
    }

    /// Starts over with these files, such as an archive opened from another app while the flow is showing. While an
    /// extraction runs, or once the flow ended, it discards them instead.
    public func choose(_ urls: [URL]) {
        // The flow can end before this task runs.
        Task {
            guard let importer, onFinish != nil, importer.step != .extracting else { return ArchiveImporter.discard(urls) }
            focus = nil
            await importer.choose(urls)
        }
    }

    /// Cancels a running extraction, or ends the flow and reports what was imported. Later calls do nothing.
    public func close() {
        guard let onFinish else { return }
        guard let importer else {
            self.onFinish = nil
            return onFinish(nil)
        }
        if importer.step == .extracting {
            importer.cancel()
            return
        }
        var imported: String?
        if case let .finished(result) = importer.step { imported = result.folderName }
        importer.close()
        self.onFinish = nil
        onFinish(imported)
    }

    func unlock() {
        guard !password.isEmpty else { return }
        importer?.submitPassword(password)
        password = ""
    }

    /// The controls on screen for the current step, top to bottom, each row left to right.
    private var rows: [[Control]] {
        guard let importer else { return [] }
        switch importer.step {
        case .choosing: return [[.chooseArchive]]
        case .opening: return []
        case .reviewing: return [[.folderName]] + (importer.canDeleteArchive ? [[.deleteToggle]] : []) + [[.chooseAnother, .startImport]]
        case .password: return [[.passwordField], [.cancelPassword, .unlock]]
        case .extracting: return [[.cancelImport]]
        case .nested: return [[.keepNested, .extractNested]]
        case .finished: return [[.importAnother, .done]]
        }
    }

    /// The focused control, or the step's main action, which is always last.
    private var current: Control? {
        let rows = rows
        if let focus, rows.contains(where: { $0.contains(focus) }) { return focus }
        return rows.last?.last
    }

    func isFocused(_ control: Control) -> Bool { showsFocus && current == control }

    public func handle(_ button: Button) {
        if button == .cancel { return close() }
        let rows = rows
        guard let current, let r = rows.firstIndex(where: { $0.contains(current) }), let c = rows[r].firstIndex(of: current) else { return }
        // The first press only reveals the ring where focus already is.
        guard showsFocus else {
            showsFocus = true
            if button == .confirm { activate(current) }
            return
        }
        switch button {
        case .up where r > 0: focus = rows[r - 1][min(c, rows[r - 1].count - 1)]
        case .down where r < rows.count - 1: focus = rows[r + 1][min(c, rows[r + 1].count - 1)]
        case .left: focus = rows[r][max(0, c - 1)]
        case .right: focus = rows[r][min(rows[r].count - 1, c + 1)]
        case .confirm: activate(current)
        default: break
        }
    }

    /// Touch and pointer: hide the ring, then activate the control.
    func select(_ control: Control) {
        showsFocus = false
        activate(control)
    }

    private func activate(_ control: Control) {
        guard let importer else { return }
        switch control {
        case .chooseArchive: picking = true
        case .folderName, .passwordField:
            editing = control
            return
        case .deleteToggle:
            importer.deleteArchiveAfterImport.toggle()
            return
        case .chooseAnother, .importAnother: importer.reset()
        case .startImport: importer.startImport()
        case .cancelPassword, .cancelImport: importer.cancel()
        case .unlock: unlock()
        case .keepNested: importer.keepNested()
        case .extractNested: importer.extractNested()
        case .done: close()
        }
        // The next step starts on its main action.
        focus = nil
    }
}

/// The whole import flow: pick an archive, name the game, enter a password if needed, watch progress, see the result.
/// Present it in a sheet. `onFinish` gets the new folder's name, or nil when nothing was imported, and the presenter
/// rescans the library and focuses that game.
public struct ArchiveImportView: View {
    let initialSelection: [URL]
    let onFinish: (String?) -> Void

    @State private var model: ArchiveImportModel

    /// `initialSelection` starts the flow with files already chosen, such as ones dropped on `ArchiveDropTarget`.
    public init(gamesRoot: URL, initialSelection: [URL] = [], onFinish: @escaping (String?) -> Void) {
        self.init(model: ArchiveImportModel(gamesRoot: gamesRoot), initialSelection: initialSelection, onFinish: onFinish)
    }

    /// For hosts that forward controller input to `model`.
    public init(model: ArchiveImportModel, initialSelection: [URL] = [], onFinish: @escaping (String?) -> Void) {
        _model = State(initialValue: model)
        self.initialSelection = initialSelection
        self.onFinish = onFinish
    }

    public var body: some View {
        let importer = model.importer
        VStack(alignment: .leading, spacing: 0) {
            HStack(alignment: .firstTextBaseline) {
                VStack(alignment: .leading, spacing: 4) {
                    Text("Import Game").font(.system(size: 22, weight: .heavy).width(.expanded))
                    if let name = importer?.source?.fileName {
                        Text(name).font(.system(size: 14)).foregroundStyle(ImportTheme.secondaryText).lineLimit(1)
                    }
                }
                Spacer()
                Button(importer?.step == .extracting ? "Cancel" : "Close", action: model.close)
                    .buttonStyle(ImportButtonStyle(kind: .secondary))
                    .keyboardShortcut(.cancelAction)
            }
            .padding(22)
            ScrollView {
                if let importer {
                    StepView(model: model, importer: importer)
                        .padding(.horizontal, 22)
                        .padding(.bottom, 28)
                        .frame(maxWidth: 640)
                        .frame(maxWidth: .infinity)
                }
            }
        }
        .foregroundStyle(.white)
        .background(ImportTheme.background.opacity(0.98))
        .preferredColorScheme(.dark)
        #if os(macOS)
        .frame(minWidth: 560, idealWidth: 640, minHeight: 520)
        #endif
        .presentationDetents([.large])
        .interactiveDismissDisabled(importer?.step == .extracting)
        // The system presents this picker modally. On an iPhone, Open did nothing in the UIDocumentPickerViewController
        // this replaced, which was embedded in a sheet inside this sheet. `.data` rather than archive types, because
        // old-style RAR volumes (.r00) and split parts (.001) have no type. The URLs are opened in place and
        // security-scoped, which `choose` handles.
        .fileImporter(isPresented: $model.picking, allowedContentTypes: [.data], allowsMultipleSelection: true) { result in
            switch result {
            case let .success(urls):
                importLog.notice("Picker returned \(urls.count) files")
                if !urls.isEmpty { model.choose(urls) }
            case let .failure(error):
                importLog.error("Picker failed: \(error.localizedDescription, privacy: .public)")
            }
        } onCancellation: {
            importLog.notice("Picker cancelled")
        }
        .task { await model.start(initialSelection: initialSelection, onFinish: onFinish) }
    }
}

/// A drop area for archives, for the hub to place where dropping a game makes sense. Pass the dropped files to
/// `ArchiveImportView(initialSelection:)`, or to `ArchiveImporter.discard` when they won't be imported.
public struct ArchiveDropTarget: View {
    let onDrop: ([URL]) -> Void
    @State private var targeted = false

    public init(onDrop: @escaping ([URL]) -> Void) { self.onDrop = onDrop }

    public var body: some View {
        VStack(spacing: 10) {
            Image(systemName: "archivebox")
                .font(.system(size: 34, weight: .semibold))
                .foregroundStyle(targeted ? ImportTheme.accent : ImportTheme.secondaryText)
            Text("Drop a zip, RAR, or 7z archive").font(.system(size: 16, weight: .semibold))
            Text("For a multi-part archive, drop all of its parts.")
                .font(.system(size: 13))
                .foregroundStyle(ImportTheme.tertiaryText)
        }
        .multilineTextAlignment(.center)
        .padding(28)
        .frame(maxWidth: .infinity, minHeight: 170)
        .background(targeted ? ImportTheme.accent.opacity(0.14) : ImportTheme.surface, in: .rect(cornerRadius: 18, style: .continuous))
        .overlay {
            RoundedRectangle(cornerRadius: 18, style: .continuous)
                .strokeBorder(targeted ? ImportTheme.accent : ImportTheme.stroke, style: StrokeStyle(lineWidth: 2, dash: [8, 6]))
        }
        .animation(.easeOut(duration: 0.15), value: targeted)
        .onArchiveDrop(isTargeted: { targeted = $0 }, perform: onDrop)
    }
}

private struct StepView: View {
    @Bindable var model: ArchiveImportModel
    @Bindable var importer: ArchiveImporter
    @FocusState private var fieldFocus: ArchiveImportModel.Control?

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            switch importer.step {
            case .choosing: choosing
            case .opening: opening
            case .reviewing: reviewing
            case let .password(wrong): passwordPrompt(wrong: wrong)
            case .extracting: extracting
            case let .nested(fileName): nested(fileName)
            case let .finished(result): finished(result)
            }
        }
        .animation(.easeInOut(duration: 0.2), value: importer.step)
        .onChange(of: model.editing) { _, editing in fieldFocus = editing }
        .onChange(of: fieldFocus) { _, focus in model.editing = focus }
    }

    private var choosing: some View {
        Group {
            ArchiveDropTarget(onDrop: model.choose)
            errorLabel
            Button { model.select(.chooseArchive) } label: { Label("Choose Archive", systemImage: "folder") }
                .buttonStyle(ImportButtonStyle(kind: .primary, focused: model.isFocused(.chooseArchive)))
                .keyboardShortcut(.defaultAction)
            Text("Games are extracted into VNDegen's Games folder. The archive itself is read in place and not copied first, so it can stay in iCloud Drive or on a server connected in Files.")
                .font(.system(size: 13))
                .foregroundStyle(ImportTheme.tertiaryText)
        }
    }

    private var opening: some View {
        HStack(spacing: 12) {
            ProgressView().controlSize(.small)
            Text("Reading the archive…").foregroundStyle(ImportTheme.secondaryText)
        }
        .padding(.vertical, 40)
        .frame(maxWidth: .infinity)
    }

    @ViewBuilder private var reviewing: some View {
        if let source = importer.source, let contents = importer.contents {
            Card {
                Label(source.fileName, systemImage: "archivebox.fill")
                    .font(.system(size: 16, weight: .semibold))
                    .lineLimit(2)
                Text(summary(files: contents.fileCount, bytes: contents.byteCount)
                     + (source.volumes.count > 1 ? ", \(source.volumes.count) parts" : "")
                     + (contents.isEncrypted ? ", password protected" : ""))
                    .font(.system(size: 14))
                    .foregroundStyle(ImportTheme.secondaryText)
            }
        }
        VStack(alignment: .leading, spacing: 8) {
            SectionTitle("Folder name")
            TextField("Folder name", text: $importer.folderName)
                .textFieldStyle(.plain)
                .font(.system(size: 17, weight: .semibold))
                .autocorrectionDisabled()
                .focused($fieldFocus, equals: .folderName)
                .padding(14)
                .fieldBackground(ImportTheme.surface, focused: model.isFocused(.folderName))
                .onSubmit { importer.startImport() }
            if let problem = importer.folderNameProblem {
                Label(problem, systemImage: "exclamationmark.circle.fill")
                    .font(.system(size: 13))
                    .foregroundStyle(ImportTheme.accent)
            } else {
                Text("The game appears in the library under this name.")
                    .font(.system(size: 13))
                    .foregroundStyle(ImportTheme.tertiaryText)
            }
        }
        if importer.canDeleteArchive {
            Toggle(isOn: $importer.deleteArchiveAfterImport) {
                VStack(alignment: .leading, spacing: 2) {
                    Text("Delete archive after import").font(.system(size: 15, weight: .semibold))
                    Text("Frees the space the archive uses on this device.")
                        .font(.system(size: 13))
                        .foregroundStyle(ImportTheme.tertiaryText)
                }
            }
            .tint(ImportTheme.accent)
            .padding(12)
            .background {
                // A Toggle has no button style, so its focus ring is drawn around the row.
                let focused = model.isFocused(.deleteToggle)
                RoundedRectangle(cornerRadius: 14, style: .continuous)
                    .fill(focused ? ImportTheme.accent.opacity(0.32) : .clear)
                    .strokeBorder(focused ? .white : .clear, lineWidth: 2.5)
            }
            .padding(.horizontal, -12)
        }
        errorLabel
        HStack(spacing: 12) {
            Button("Choose Another") { model.select(.chooseAnother) }
                .buttonStyle(ImportButtonStyle(kind: .secondary, focused: model.isFocused(.chooseAnother)))
            Spacer()
            Button { model.select(.startImport) } label: { Label("Import", systemImage: "square.and.arrow.down") }
                .buttonStyle(ImportButtonStyle(kind: .primary, focused: model.isFocused(.startImport)))
                .keyboardShortcut(.defaultAction)
                .disabled(importer.folderNameProblem != nil)
        }
    }

    private func passwordPrompt(wrong: Bool) -> some View {
        Group {
            Card {
                Label("Password required", systemImage: "lock.fill").font(.system(size: 17, weight: .bold))
                Text("Enter the password for \(importer.source?.fileName ?? "this archive").")
                    .font(.system(size: 14))
                    .foregroundStyle(ImportTheme.secondaryText)
                SecureField("Password", text: $model.password)
                    .textFieldStyle(.plain)
                    .font(.system(size: 17))
                    .focused($fieldFocus, equals: .passwordField)
                    .padding(14)
                    .fieldBackground(Color.black.opacity(0.3), focused: model.isFocused(.passwordField),
                                     stroke: wrong ? ImportTheme.accent : ImportTheme.stroke)
                    .onSubmit(model.unlock)
                if wrong {
                    Label("That password is not correct. Try again.", systemImage: "exclamationmark.circle.fill")
                        .font(.system(size: 13))
                        .foregroundStyle(ImportTheme.accent)
                }
            }
            HStack(spacing: 12) {
                Button("Cancel") { model.select(.cancelPassword) }
                    .buttonStyle(ImportButtonStyle(kind: .secondary, focused: model.isFocused(.cancelPassword)))
                Spacer()
                Button { model.select(.unlock) } label: { Label("Unlock", systemImage: "lock.open.fill") }
                    .buttonStyle(ImportButtonStyle(kind: .primary, focused: model.isFocused(.unlock)))
                    .keyboardShortcut(.defaultAction)
                    .disabled(model.password.isEmpty)
            }
        }
    }

    @ViewBuilder private var extracting: some View {
        let progress = importer.progress
        Card {
            Text("Importing \(importer.folderName)")
                .font(.system(size: 17, weight: .bold))
                .lineLimit(2)
            ProgressView(value: progress?.fraction ?? 0)
                .tint(ImportTheme.accent)
            HStack {
                Text("\((progress?.bytes ?? 0).formatted(.byteCount(style: .file))) of \((progress?.total.byteCount ?? 0).formatted(.byteCount(style: .file)))")
                Spacer()
                Text("\((progress?.files ?? 0).formatted()) of \((progress?.total.fileCount ?? 0).formatted()) files")
            }
            .font(.system(size: 14).monospacedDigit())
            .foregroundStyle(ImportTheme.secondaryText)
        }
        Text("Keep VNDegen open until the import finishes. The screen stays on meanwhile.")
            .font(.system(size: 13))
            .foregroundStyle(ImportTheme.tertiaryText)
        Button("Cancel Import", role: .cancel) { model.select(.cancelImport) }
            .buttonStyle(ImportButtonStyle(kind: .secondary, focused: model.isFocused(.cancelImport)))
    }

    private func nested(_ fileName: String) -> some View {
        Group {
            Card {
                Label("This archive holds another archive", systemImage: "shippingbox.fill")
                    .font(.system(size: 17, weight: .bold))
                Text(fileName).font(.system(size: 14, weight: .semibold)).lineLimit(2)
                Text("Extract it too, so the game folder holds the game instead of a second archive?")
                    .font(.system(size: 14))
                    .foregroundStyle(ImportTheme.secondaryText)
            }
            HStack(spacing: 12) {
                Button("Keep As Is") { model.select(.keepNested) }
                    .buttonStyle(ImportButtonStyle(kind: .secondary, focused: model.isFocused(.keepNested)))
                Spacer()
                Button { model.select(.extractNested) } label: { Label("Extract", systemImage: "arrow.down.doc.fill") }
                    .buttonStyle(ImportButtonStyle(kind: .primary, focused: model.isFocused(.extractNested)))
                    .keyboardShortcut(.defaultAction)
            }
        }
    }

    private func finished(_ result: ArchiveImporter.ImportResult) -> some View {
        Group {
            Card {
                Label("\(result.folderName) is in your library", systemImage: "checkmark.circle.fill")
                    .font(.system(size: 17, weight: .bold))
                    .symbolRenderingMode(.multicolor)
                Text("\(summary(files: result.files, bytes: result.bytes)) in \(result.duration.formatted(.units(allowed: [.hours, .minutes, .seconds], width: .abbreviated)))")
                    .font(.system(size: 14))
                    .foregroundStyle(ImportTheme.secondaryText)
            }
            HStack(spacing: 12) {
                Button("Import Another") { model.select(.importAnother) }
                    .buttonStyle(ImportButtonStyle(kind: .secondary, focused: model.isFocused(.importAnother)))
                Spacer()
                Button("Done") { model.select(.done) }
                    .buttonStyle(ImportButtonStyle(kind: .primary, focused: model.isFocused(.done)))
                    .keyboardShortcut(.defaultAction)
            }
        }
    }

    @ViewBuilder private var errorLabel: some View {
        if let message = importer.errorMessage {
            Label(message, systemImage: "exclamationmark.triangle.fill")
                .font(.system(size: 14))
                .foregroundStyle(ImportTheme.accent)
        }
    }

    private func summary(files: Int, bytes: Int64) -> String {
        "\(files.formatted()) \(files == 1 ? "file" : "files"), \(bytes.formatted(.byteCount(style: .file)))"
    }
}

private struct Card<Content: View>: View {
    @ViewBuilder var content: Content

    var body: some View {
        VStack(alignment: .leading, spacing: 10) { content }
            .padding(18)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(ImportTheme.surface, in: .rect(cornerRadius: 16, style: .continuous))
            .overlay { RoundedRectangle(cornerRadius: 16, style: .continuous).strokeBorder(ImportTheme.stroke) }
    }
}

private extension View {
    /// A text field's rounded background, outlined in white while the controller focuses it.
    func fieldBackground(_ fill: Color, focused: Bool, stroke: Color = ImportTheme.stroke) -> some View {
        let shape = RoundedRectangle(cornerRadius: 12, style: .continuous)
        return background(focused ? Color.white.opacity(0.16) : fill, in: shape)
            .overlay { shape.strokeBorder(focused ? .white : stroke, lineWidth: focused ? 2 : 1) }
            .animation(.easeOut(duration: 0.15), value: focused)
    }
}

private struct SectionTitle: View {
    let text: String
    init(_ text: String) { self.text = text }

    var body: some View {
        Text(text.uppercased())
            .font(.system(size: 12, weight: .bold).width(.expanded))
            .tracking(1.6)
            .foregroundStyle(ImportTheme.secondaryText)
    }
}

/// The hub's palette and button shapes, copied from HubUI's Theme and HubButtonStyle so this module stays independent.
enum ImportTheme {
    static let background = Color(red: 0.035, green: 0.035, blue: 0.06)
    static let surface = Color.white.opacity(0.07)
    static let stroke = Color.white.opacity(0.12)
    static let accent = Color(red: 1.0, green: 0.25, blue: 0.5)
    static let secondaryText = Color.white.opacity(0.64)
    static let tertiaryText = Color.white.opacity(0.42)
}

struct ImportButtonStyle: ButtonStyle {
    enum Kind { case primary, secondary }
    let kind: Kind
    var focused = false
    @Environment(\.isEnabled) private var isEnabled

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: kind == .primary ? 17 : 15, weight: .bold))
            .foregroundStyle(kind == .secondary && focused ? .black : .white)
            .padding(.horizontal, kind == .primary ? 28 : 18)
            .frame(minHeight: kind == .primary ? 52 : 44)
            .background {
                switch kind {
                case .primary: Capsule().fill(ImportTheme.accent.gradient)
                case .secondary:
                    if focused { Capsule().fill(.white) } else { Capsule().fill(.ultraThinMaterial).overlay(Capsule().fill(.white.opacity(0.06))) }
                }
            }
            .overlay { Capsule().stroke(focused ? .white : ImportTheme.stroke, lineWidth: focused ? (kind == .primary ? 3 : 0) : 1) }
            .contentShape(Capsule())
            .opacity(isEnabled ? 1 : 0.4)
            .scaleEffect(configuration.isPressed ? 0.95 : (focused ? 1.06 : 1))
            .shadow(color: focused ? ImportTheme.accent.opacity(kind == .primary ? 0.7 : 0.35) : .clear, radius: 16)
            .animation(.spring(response: 0.28, dampingFraction: 0.78), value: focused)
            .animation(.spring(response: 0.28, dampingFraction: 0.78), value: configuration.isPressed)
    }
}
