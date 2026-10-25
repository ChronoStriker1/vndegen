import SwiftUI
import VNCore
import GameInput

/// Hosts a running engine session full screen, with touch controls and the hub menu over it.
struct PlayerView: View {
    let hub: Hub
    let session: any EngineSession
    @State private var menu = PlayerMenu()
    @AppStorage(TouchControlsMode.storageKey) private var touchMode = TouchControlsMode.gamepad

    var body: some View {
        let showsTouchControls = hub.input.lastInputKind == .touch && hub.input.controllerNames.isEmpty && !menu.isOpen
        ZStack {
            SessionViewHost(session: session)
                .ignoresSafeArea()
            if showsTouchControls {
                TouchControlsOverlay(mode: $touchMode) { event in
                    hub.input.noteInput(.touch)
                    handle(event)
                }
            }
            if menu.isOpen {
                PlayerMenuView(menu: menu, title: title, onResume: resume, onQuit: hub.endSession)
                    .transition(.opacity)
            } else {
                Button { openMenu() } label: {
                    Image(systemName: "square.grid.2x2.fill")
                        .font(.system(size: 14, weight: .bold))
                        .frame(width: 36, height: 36)
                        .background(.ultraThinMaterial, in: .circle)
                        .opacity(0.55)
                        .frame(width: 44, height: 44)
                        .contentShape(.circle)
                }
                .buttonStyle(.plain)
                .accessibilityLabel("VNDegen menu")
                .padding(10)
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topTrailing)
            }
        }
        .animation(.easeOut(duration: 0.2), value: menu.isOpen)
        .background(.black)
        .onAppear {
            hub.input.onEvent = handle
            applyInputMode()
            #if os(iOS)
            OrientationLock.apply(game?.orientation ?? PlayOrientation.setting)
            #endif
        }
        #if os(iOS)
        .onDisappear { OrientationLock.apply(nil) }
        #endif
    }

    private var game: Game? {
        guard case let .playing(id, _) = hub.screen else { return nil }
        return hub.library.game(id)
    }

    private var title: String { game.map(hub.displayTitle) ?? "" }

    private func handle(_ event: ControllerEvent) {
        guard menu.isOpen else {
            if event == .button(.hub, pressed: true) { openMenu() } else { session.send(event) }
            return
        }
        guard case let .button(button, pressed: true) = event else { return }
        switch button {
        case .up, .left: menu.focus = .resume
        case .down, .right: menu.focus = .quit
        case .confirm: menu.focus == .resume ? resume() : hub.endSession()
        case .cancel, .hub, .menu: resume()
        default: break
        }
    }

    private func openMenu() {
        session.setPaused(true)
        menu.focus = .resume
        menu.isOpen = true
        applyInputMode()
    }

    private func resume() {
        menu.isOpen = false
        applyInputMode()
        session.setPaused(false)
    }

    /// The game reads the keyboard itself and wants raw presses. The menu wants translated, repeating navigation.
    private func applyInputMode() {
        hub.input.translatesKeyboard = menu.isOpen
        hub.input.repeatsDirections = menu.isOpen
    }
}

@MainActor
@Observable
final class PlayerMenu {
    enum Item { case resume, quit }
    var isOpen = false
    var focus = Item.resume
}

private struct PlayerMenuView: View {
    let menu: PlayerMenu
    let title: String
    let onResume: () -> Void
    let onQuit: () -> Void

    var body: some View {
        ZStack {
            Rectangle().fill(.black.opacity(0.55)).ignoresSafeArea()
                .onTapGesture(perform: onResume)
            VStack(spacing: 14) {
                Wordmark().padding(.bottom, 4)
                Text(title)
                    .font(.system(size: 15, weight: .semibold))
                    .foregroundStyle(Theme.secondaryText)
                    .lineLimit(1)
                Button(action: onResume) { Label("Resume", systemImage: "play.fill").frame(minWidth: 220) }
                    .buttonStyle(HubButtonStyle(kind: .primary, focused: menu.focus == .resume))
                    .onHover { if $0 { menu.focus = .resume } }
                Button(action: onQuit) { Label("Quit to library", systemImage: "xmark").frame(minWidth: 220) }
                    .buttonStyle(HubButtonStyle(kind: .secondary, focused: menu.focus == .quit))
                    .onHover { if $0 { menu.focus = .quit } }
            }
            .padding(28)
            .background(.ultraThinMaterial, in: .rect(cornerRadius: 24, style: .continuous))
            .overlay { RoundedRectangle(cornerRadius: 24, style: .continuous).strokeBorder(Theme.stroke) }
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
