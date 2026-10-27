import SwiftUI
import VNCore
import ControllerUI

/// The state behind `MetadataSettingsView`, including gamepad focus. A host that drives the view with a controller keeps
/// the model and forwards button presses to `handle(_:)`, or to `keyboard` while it is open. A controller types the keys
/// on that on-screen keyboard.
@MainActor
@Observable
public final class MetadataSettingsModel {
    enum Target: Hashable {
        case field(Credential)
        case save(MetadataSource), clear(MetadataSource)
    }

    enum Check {
        case running, passed
        case failed(String)
    }

    let service: MetadataService
    var values: [Credential: String] = [:]
    var checks: [MetadataSource: Check] = [:]
    var focus = FocusRing<Target>()
    public private(set) var keyboard: OnScreenKeyboard?

    /// The sources that take keys, with their fields in screen order.
    static let keyedSources: [(source: MetadataSource, fields: [Credential])] = [
        (.steamGridDB, [.steamGridDBKey]),
        (.igdb, [.igdbClientID, .igdbClientSecret]),
    ]

    public init(service: MetadataService) {
        self.service = service
        reload()
    }

    /// Reads the stored keys.
    private func reload() {
        values = Dictionary(uniqueKeysWithValues: Credential.allCases.map { ($0, service.credential($0)) })
        checks = [:]
    }

    var rows: [[Target]] {
        Self.keyedSources.flatMap { source, fields in fields.map { [Target.field($0)] } + [[.save(source), .clear(source)]] }
    }

    func isFocused(_ target: Target) -> Bool { focus.isFocused(target, in: rows) }

    func values(for source: MetadataSource) -> [Credential: String] {
        let fields = Self.keyedSources.first { $0.source == source }?.fields ?? []
        return Dictionary(uniqueKeysWithValues: fields.map { ($0, values[$0] ?? "") })
    }

    func canSave(_ source: MetadataSource) -> Bool {
        !values(for: source).values.contains { $0.trimmingCharacters(in: .whitespaces).isEmpty }
    }

    func canClear(_ source: MetadataSource) -> Bool { service.activeSources.contains(source) }

    /// Directions move focus and confirm activates it. Returns false for buttons the host should handle, such as cancel.
    public func handle(_ button: VirtualButton) -> Bool {
        switch button {
        case .confirm:
            focus.showsFocus = true
            if let target = focus.current(in: rows) { activate(target) }
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
        switch target {
        case let .field(credential):
            keyboard = OnScreenKeyboard(title: credential.title, text: values[credential] ?? "", isSecure: true) { [weak self] in
                self?.values[credential] = $0
            } onClose: { [weak self] in
                self?.keyboard = nil
            }
        case let .save(source):
            guard canSave(source) else { return }
            save(values(for: source), testing: source)
        case let .clear(source):
            guard canClear(source) else { return }
            clear(source, values(for: source).keys)
        }
    }

    private func save(_ values: [Credential: String], testing source: MetadataSource) {
        do {
            try service.setCredentials(values)
        } catch {
            checks[source] = .failed(error.localizedDescription)
            return
        }
        checks[source] = .running
        Task {
            do {
                try await service.checkAccess(source)
                checks[source] = .passed
            } catch {
                checks[source] = .failed(error.localizedDescription)
            }
        }
    }

    private func clear(_ source: MetadataSource, _ credentials: some Sequence<Credential>) {
        do {
            try service.setCredentials(Dictionary(uniqueKeysWithValues: credentials.map { ($0, "") }))
            for credential in credentials { values[credential] = "" }
            checks[source] = nil
        } catch {
            checks[source] = .failed(error.localizedDescription)
        }
    }
}

/// API keys for IGDB (Twitch client ID and secret) and SteamGridDB. Shown inside the app's Settings screen.
public struct MetadataSettingsView: View {
    @State private var model: MetadataSettingsModel

    public init(service: MetadataService) { _model = State(initialValue: MetadataSettingsModel(service: service)) }
    public init(model: MetadataSettingsModel) { _model = State(initialValue: model) }

    public var body: some View {
        ScrollViewReader { proxy in
            Form {
                Section {
                    ForEach([MetadataSource.vndb, .steam, .steamGridDB, .igdb], id: \.self) { source in
                        HStack {
                            Text(source.displayName)
                            Spacer()
                            if model.service.activeSources.contains(source) {
                                Label("Active", systemImage: "checkmark.circle.fill").foregroundStyle(.green)
                            } else {
                                Label("Needs a key", systemImage: "key").foregroundStyle(.secondary)
                            }
                        }
                    }
                } header: {
                    Text("Sources")
                } footer: {
                    Text("VNDB and Steam need no key. Sources without keys are skipped.")
                }

                Section {
                    field(.steamGridDBKey)
                    actions(.steamGridDB)
                } header: {
                    Text("SteamGridDB")
                } footer: {
                    Text("[Create a key in your SteamGridDB preferences](https://www.steamgriddb.com/profile/preferences/api)")
                }

                Section {
                    // Both fields are secure so the values never show up in screenshots or screen sharing.
                    field(.igdbClientID)
                    field(.igdbClientSecret)
                    actions(.igdb)
                } header: {
                    Text("IGDB")
                } footer: {
                    Text("[Register an app in the Twitch developer console](https://dev.twitch.tv/console/apps)")
                }
            }
            .formStyle(.grouped)
            .onChange(of: model.focus.target) { _, target in
                guard model.focus.showsFocus, let target else { return }
                withAnimation { proxy.scrollTo(target, anchor: .center) }
            }
        }
        .navigationTitle("Metadata Sources")
        .onScreenKeyboard(model.keyboard)
    }

    private func field(_ credential: Credential) -> some View {
        SecureField(credential.title, text: Binding(get: { model.values[credential] ?? "" }, set: { model.values[credential] = $0 }))
            .focusRow(model.isFocused(.field(credential)), id: MetadataSettingsModel.Target.field(credential))
    }

    @ViewBuilder
    private func actions(_ source: MetadataSource) -> some View {
        HStack {
            Button("Save and Test") { model.select(.save(source)) }
                .disabled(!model.canSave(source))
                .padding(.horizontal, 8)
                .padding(.vertical, 4)
                .background { if model.isFocused(.save(source)) { FocusRowBackground() } }
            Spacer()
            Button("Clear", role: .destructive) { model.select(.clear(source)) }
                .disabled(!model.canClear(source))
                .padding(.horizontal, 8)
                .padding(.vertical, 4)
                .background { if model.isFocused(.clear(source)) { FocusRowBackground() } }
        }
        .buttonStyle(.borderless)
        .id(MetadataSettingsModel.Target.save(source))

        switch model.checks[source] {
        case .running:
            HStack {
                ProgressView()
                Text("Testing")
            }
        case .passed:
            Label("Connected", systemImage: "checkmark.circle.fill").foregroundStyle(.green)
        case let .failed(message):
            Label(message, systemImage: "xmark.octagon.fill").foregroundStyle(.red)
        case nil:
            EmptyView()
        }
    }
}

extension Credential {
    var title: String {
        switch self {
        case .steamGridDBKey: "API key"
        case .igdbClientID: "Twitch client ID"
        case .igdbClientSecret: "Twitch client secret"
        }
    }
}
