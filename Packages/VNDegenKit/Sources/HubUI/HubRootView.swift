import SwiftUI
import VNCore

/// Placeholder library screen. The Big Picture-style hub replaces it.
public struct HubRootView: View {
    @Bindable var hub: Hub

    public init(hub: Hub) { self.hub = hub }

    public var body: some View {
        switch hub.screen {
        case .library:
            NavigationStack {
                List(hub.library.games) { game in
                    Button {
                        hub.launch(game.id)
                    } label: {
                        LabeledContent(game.title, value: hub.library.plugin(for: game.engine)?.descriptor.shortName ?? "Unknown")
                    }
                }
                .navigationTitle("Library")
                .refreshable { await hub.library.refresh() }
            }
            .alert("Can't launch", isPresented: Binding(get: { hub.launchError != nil }, set: { if !$0 { hub.launchError = nil } })) {
                Button("OK") {}
            } message: {
                Text(hub.launchError ?? "")
            }
        case let .playing(_, session):
            PlayerView(hub: hub, session: session)
        }
    }
}
