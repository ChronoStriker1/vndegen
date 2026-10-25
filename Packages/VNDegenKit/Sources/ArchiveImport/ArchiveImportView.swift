import SwiftUI
import UniformTypeIdentifiers

/// The whole import flow: pick an archive, name the game, enter a password if needed, watch progress, see the result.
/// Present it in a sheet. `onFinish` gets the new folder's name, or nil when nothing was imported, and the presenter
/// rescans the library and focuses that game.
public struct ArchiveImportView: View {
    let gamesRoot: URL
    let initialSelection: [URL]
    let onFinish: (String?) -> Void

    @State private var importer: ArchiveImporter?

    /// `initialSelection` starts the flow with files already chosen, such as ones dropped on `ArchiveDropTarget`.
    public init(gamesRoot: URL, initialSelection: [URL] = [], onFinish: @escaping (String?) -> Void) {
        self.gamesRoot = gamesRoot
        self.initialSelection = initialSelection
        self.onFinish = onFinish
    }

    public var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack(alignment: .firstTextBaseline) {
                VStack(alignment: .leading, spacing: 4) {
                    Text("Import Game").font(.system(size: 22, weight: .heavy).width(.expanded))
                    if let name = importer?.source?.fileName {
                        Text(name).font(.system(size: 14)).foregroundStyle(ImportTheme.secondaryText).lineLimit(1)
                    }
                }
                Spacer()
                Button(importer?.step == .extracting ? "Cancel" : "Close", action: close)
                    .buttonStyle(ImportButtonStyle(kind: .secondary))
                    .keyboardShortcut(.cancelAction)
            }
            .padding(22)
            ScrollView {
                if let importer {
                    StepView(importer: importer, onDone: close)
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
        .task {
            guard importer == nil else { return }
            let importer = ArchiveImporter(gamesRoot: gamesRoot)
            self.importer = importer
            if !initialSelection.isEmpty { await importer.choose(initialSelection) }
        }
    }

    private func close() {
        guard let importer else { return onFinish(nil) }
        if importer.step == .extracting {
            importer.cancel()
            return
        }
        var imported: String?
        if case let .finished(result) = importer.step { imported = result.folderName }
        importer.close()
        onFinish(imported)
    }
}

/// A drop area for archives, for the hub to place where dropping a game makes sense. Pass the dropped files to
/// `ArchiveImportView(initialSelection:)`.
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
        .dropDestination(for: URL.self) { urls, _ in
            onDrop(urls)
            return !urls.isEmpty
        } isTargeted: { targeted = $0 }
    }
}

private struct StepView: View {
    @Bindable var importer: ArchiveImporter
    let onDone: () -> Void
    @State private var picking = false
    @State private var password = ""

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
        #if os(iOS)
        .sheet(isPresented: $picking) {
            DocumentPicker { urls in
                picking = false
                if !urls.isEmpty { Task { await importer.choose(urls) } }
            }
            .ignoresSafeArea()
        }
        #else
        .fileImporter(isPresented: $picking, allowedContentTypes: [.data], allowsMultipleSelection: true) { result in
            if case let .success(urls) = result, !urls.isEmpty { Task { await importer.choose(urls) } }
        }
        #endif
    }

    private var choosing: some View {
        Group {
            ArchiveDropTarget { urls in Task { await importer.choose(urls) } }
            errorLabel
            Button { picking = true } label: { Label("Choose Archive", systemImage: "folder") }
                .buttonStyle(ImportButtonStyle(kind: .primary))
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
                .padding(14)
                .background(ImportTheme.surface, in: .rect(cornerRadius: 12, style: .continuous))
                .overlay { RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(ImportTheme.stroke) }
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
        if importer.archiveIsOnDevice {
            Toggle(isOn: $importer.deleteArchiveAfterImport) {
                VStack(alignment: .leading, spacing: 2) {
                    Text("Delete archive after import").font(.system(size: 15, weight: .semibold))
                    Text("Frees the space the archive uses on this device.")
                        .font(.system(size: 13))
                        .foregroundStyle(ImportTheme.tertiaryText)
                }
            }
            .tint(ImportTheme.accent)
        }
        errorLabel
        HStack(spacing: 12) {
            Button("Choose Another") { importer.reset() }
                .buttonStyle(ImportButtonStyle(kind: .secondary))
            Spacer()
            Button { importer.startImport() } label: { Label("Import", systemImage: "square.and.arrow.down") }
                .buttonStyle(ImportButtonStyle(kind: .primary))
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
                SecureField("Password", text: $password)
                    .textFieldStyle(.plain)
                    .font(.system(size: 17))
                    .padding(14)
                    .background(Color.black.opacity(0.3), in: .rect(cornerRadius: 12, style: .continuous))
                    .overlay {
                        RoundedRectangle(cornerRadius: 12, style: .continuous)
                            .strokeBorder(wrong ? ImportTheme.accent : ImportTheme.stroke)
                    }
                    .onSubmit(unlock)
                if wrong {
                    Label("That password is not correct. Try again.", systemImage: "exclamationmark.circle.fill")
                        .font(.system(size: 13))
                        .foregroundStyle(ImportTheme.accent)
                }
            }
            HStack(spacing: 12) {
                Button("Cancel") { importer.cancel() }
                    .buttonStyle(ImportButtonStyle(kind: .secondary))
                Spacer()
                Button(action: unlock) { Label("Unlock", systemImage: "lock.open.fill") }
                    .buttonStyle(ImportButtonStyle(kind: .primary))
                    .keyboardShortcut(.defaultAction)
                    .disabled(password.isEmpty)
            }
        }
    }

    private func unlock() {
        guard !password.isEmpty else { return }
        importer.submitPassword(password)
        password = ""
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
        Button("Cancel Import", role: .cancel) { importer.cancel() }
            .buttonStyle(ImportButtonStyle(kind: .secondary))
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
                Button("Keep As Is") { importer.keepNested() }
                    .buttonStyle(ImportButtonStyle(kind: .secondary))
                Spacer()
                Button { importer.extractNested() } label: { Label("Extract", systemImage: "arrow.down.doc.fill") }
                    .buttonStyle(ImportButtonStyle(kind: .primary))
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
                Button("Import Another") { importer.reset() }
                    .buttonStyle(ImportButtonStyle(kind: .secondary))
                Spacer()
                Button("Done", action: onDone)
                    .buttonStyle(ImportButtonStyle(kind: .primary))
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
    @Environment(\.isEnabled) private var isEnabled

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: kind == .primary ? 17 : 15, weight: .bold))
            .foregroundStyle(.white)
            .padding(.horizontal, kind == .primary ? 28 : 18)
            .frame(minHeight: kind == .primary ? 52 : 44)
            .background {
                switch kind {
                case .primary: Capsule().fill(ImportTheme.accent.gradient)
                case .secondary: Capsule().fill(.ultraThinMaterial).overlay(Capsule().fill(.white.opacity(0.06)))
                }
            }
            .overlay { Capsule().stroke(ImportTheme.stroke, lineWidth: 1) }
            .contentShape(Capsule())
            .opacity(isEnabled ? 1 : 0.4)
            .scaleEffect(configuration.isPressed ? 0.95 : 1)
            .animation(.spring(response: 0.28, dampingFraction: 0.78), value: configuration.isPressed)
    }
}

#if os(iOS)
/// The system document picker, opened in place (`asCopy: false`) so a multi-GB archive is read where it is instead of
/// being copied into the app first. The picked URLs are security-scoped, which `ArchiveImporter.choose` handles.
private struct DocumentPicker: UIViewControllerRepresentable {
    let onPick: ([URL]) -> Void

    func makeUIViewController(context: Context) -> UIDocumentPickerViewController {
        // `.data` rather than archive types, because old-style RAR volumes (.r00) and split parts (.001) have no type.
        let picker = UIDocumentPickerViewController(forOpeningContentTypes: [.data], asCopy: false)
        picker.allowsMultipleSelection = true
        picker.delegate = context.coordinator
        return picker
    }

    func updateUIViewController(_ picker: UIDocumentPickerViewController, context: Context) {}

    func makeCoordinator() -> Coordinator { Coordinator(onPick: onPick) }

    final class Coordinator: NSObject, UIDocumentPickerDelegate {
        let onPick: ([URL]) -> Void
        init(onPick: @escaping ([URL]) -> Void) { self.onPick = onPick }

        func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) { onPick(urls) }
        func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) { onPick([]) }
    }
}
#endif
