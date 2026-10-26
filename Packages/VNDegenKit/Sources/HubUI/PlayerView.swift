import SwiftUI
import VNCore
import GameInput
import Lock

/// Hosts a running engine session full screen, with touch controls and the hub menu over it.
struct PlayerView: View {
    let hub: Hub
    let session: any EngineSession
    @State private var menu = PlayerMenu()
    #if os(iOS)
    @AppStorage(TouchControlsMode.storageKey) private var touchMode = TouchControlsMode.gamepad
    #endif
    @State private var needsRotation = false
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        ZStack {
            SessionViewHost(session: session)
                .ignoresSafeArea()
            #if os(iOS)
            if #available(iOS 26, *), OrientationLock.usesSceneLock {
                // Full size so every window resize lays it out again.
                SceneOrientationLock(orientation: orientation, needsRotation: $needsRotation)
                    .ignoresSafeArea()
                    .allowsHitTesting(false)
            }
            if needsRotation {
                RotateHint(orientation: orientation)
                    .transition(.opacity.combined(with: .scale(scale: 0.96)))
            }
            // Macs have no touch screen, so only iOS gets the on-screen controls.
            if hub.input.lastInputKind == .touch, hub.input.controllerNames.isEmpty, !menu.isOpen, !isCovered {
                TouchControlsOverlay(mode: $touchMode) { event in
                    hub.input.noteInput(.touch)
                    handle(event)
                }
            }
            #endif
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
            if isCovered {
                // Recreated when the app becomes active, so the Face ID prompt starts in the foreground rather than
                // failing while the app is still in the background.
                unlockView
                    .id(scenePhase == .active)
                    .transition(.opacity)
            }
        }
        .animation(.easeOut(duration: 0.2), value: menu.isOpen)
        .animation(.easeOut(duration: 0.2), value: isCovered)
        .animation(.easeOut(duration: 0.25), value: needsRotation)
        .background(.black)
        .onAppear {
            hub.input.onEvent = handle
            applyInputMode()
            #if os(iOS)
            OrientationLock.apply(orientation)
            #endif
        }
        .onChange(of: isCovered, initial: true) { _, covered in
            applyInputMode()
            if covered {
                session.setPaused(true)
            } else if !menu.isOpen {
                session.setPaused(false)
            }
        }
        #if os(iOS)
        .onDisappear { OrientationLock.apply(nil) }
        #endif
    }

    private var game: Game? {
        guard case let .playing(id, _) = hub.screen else { return nil }
        return hub.library.game(id)
    }

    private var orientation: PlayOrientation { game?.orientation ?? PlayOrientation.setting }

    private var title: String { game.map(hub.displayTitle) ?? "" }

    /// A hidden game stays paused behind the unlock screen while the library is locked, such as after the app went to
    /// the background mid-game. Ending the session instead would lose progress the game has not saved.
    private var isCovered: Bool { game?.isLocked == true && !hub.lock.isUnlocked }

    /// Unlocking resumes the game. Cancel quits to the library through the normal path, so play time is recorded.
    private var unlockView: UnlockView {
        UnlockView(lock: hub.lock) { unlocked in
            if !unlocked { hub.endSession() }
        }
    }

    private func handle(_ event: ControllerEvent) {
        if isCovered {
            unlockView.handle(event)
            return
        }
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

    /// The game reads the keyboard itself and wants raw presses. The menu and the unlock screen want translated,
    /// repeating navigation.
    private func applyInputMode() {
        hub.input.translatesKeyboard = menu.isOpen || isCovered
        hub.input.repeatsDirections = menu.isOpen || isCovered
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

/// Asks the player to turn a full-screen iPad that shows the game in the wrong orientation.
private struct RotateHint: View {
    let orientation: PlayOrientation

    var body: some View {
        let landscape = orientation == .landscape
        VStack(spacing: 12) {
            Image(systemName: landscape ? "rectangle.landscape.rotate" : "rectangle.portrait.rotate")
                .font(.system(size: 40, weight: .semibold))
                .foregroundStyle(Theme.accent)
                .symbolEffect(.wiggle.byLayer, options: .repeat(.periodic(delay: 1.5)))
            Text(landscape ? "Rotate to landscape" : "Rotate to portrait")
                .font(.display(17, weight: .heavy))
                .foregroundStyle(.white)
            Text("The game locks once it is \(landscape ? "landscape" : "portrait").")
                .font(.system(size: 14))
                .foregroundStyle(Theme.secondaryText)
        }
        .padding(.horizontal, 32)
        .padding(.vertical, 26)
        .background(.ultraThinMaterial, in: .rect(cornerRadius: 24, style: .continuous))
        .overlay { RoundedRectangle(cornerRadius: 24, style: .continuous).strokeBorder(Theme.stroke) }
        .accessibilityElement(children: .combine)
        .allowsHitTesting(false)
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
