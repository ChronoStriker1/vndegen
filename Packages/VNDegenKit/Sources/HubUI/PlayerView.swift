import SwiftUI
import VNCore

/// Hosts a running engine session full screen.
struct PlayerView: View {
    let hub: Hub
    let session: any EngineSession

    var body: some View {
        SessionViewHost(session: session)
            .ignoresSafeArea()
            .background(.black)
            .onAppear {
                hub.input.translatesKeyboard = false
                hub.input.onEvent = { event in
                    if event == .button(.hub, pressed: true) { hub.endSession() } else { session.send(event) }
                }
            }
    }
}

#if canImport(UIKit)
private struct SessionViewHost: UIViewRepresentable {
    let session: any EngineSession
    func makeUIView(context: Context) -> UIView { session.view }
    func updateUIView(_ view: UIView, context: Context) {}
}
#else
private struct SessionViewHost: NSViewRepresentable {
    let session: any EngineSession
    func makeNSView(context: Context) -> NSView { session.view }
    func updateNSView(_ view: NSView, context: Context) {}
}
#endif
