import SwiftUI
import VNCore

/// API keys for IGDB (Twitch client ID and secret) and SteamGridDB. Shown inside the app's Settings screen.
public struct MetadataSettingsView: View {
    let service: MetadataService
    @State private var steamGridDBKey = ""
    @State private var igdbClientID = ""
    @State private var igdbClientSecret = ""
    @State private var checks: [MetadataSource: Check] = [:]

    private enum Check {
        case running, passed
        case failed(String)
    }

    public init(service: MetadataService) { self.service = service }

    public var body: some View {
        Form {
            Section {
                ForEach([MetadataSource.vndb, .steam, .steamGridDB, .igdb], id: \.self) { source in
                    HStack {
                        Text(source.displayName)
                        Spacer()
                        if service.activeSources.contains(source) {
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
                SecureField("API key", text: $steamGridDBKey)
                actions(.steamGridDB, [.steamGridDBKey: steamGridDBKey])
            } header: {
                Text("SteamGridDB")
            } footer: {
                Text("[Create a key in your SteamGridDB preferences](https://www.steamgriddb.com/profile/preferences/api)")
            }

            Section {
                // Both fields are secure so the values never show up in screenshots or screen sharing.
                SecureField("Twitch client ID", text: $igdbClientID)
                SecureField("Twitch client secret", text: $igdbClientSecret)
                actions(.igdb, [.igdbClientID: igdbClientID, .igdbClientSecret: igdbClientSecret])
            } header: {
                Text("IGDB")
            } footer: {
                Text("[Register an app in the Twitch developer console](https://dev.twitch.tv/console/apps)")
            }
        }
        .formStyle(.grouped)
        .navigationTitle("Metadata Sources")
        .onAppear {
            steamGridDBKey = service.credential(.steamGridDBKey)
            igdbClientID = service.credential(.igdbClientID)
            igdbClientSecret = service.credential(.igdbClientSecret)
        }
    }

    @ViewBuilder
    private func actions(_ source: MetadataSource, _ values: [Credential: String]) -> some View {
        HStack {
            Button("Save and Test") { save(values, testing: source) }
                .disabled(values.values.contains { $0.trimmingCharacters(in: .whitespaces).isEmpty })
            Spacer()
            Button("Clear", role: .destructive) { clear(source, values.keys) }
                .disabled(!service.activeSources.contains(source))
        }
        .buttonStyle(.borderless)

        switch checks[source] {
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
            for credential in credentials {
                switch credential {
                case .steamGridDBKey: steamGridDBKey = ""
                case .igdbClientID: igdbClientID = ""
                case .igdbClientSecret: igdbClientSecret = ""
                }
            }
            checks[source] = nil
        } catch {
            checks[source] = .failed(error.localizedDescription)
        }
    }
}
