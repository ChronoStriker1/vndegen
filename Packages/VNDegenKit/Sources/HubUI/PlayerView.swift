import SwiftUI
import VNCore
import GameInput
import Lock

/// Hosts a running engine session full screen, with touch controls and the hub menu over it.
struct PlayerView: View {
    let hub: Hub
    let session: any EngineSession
    @Environment(Navigator.self) private var nav
    @State private var menu = PlayerMenu()
    @State private var mappingEditor: ButtonMappingEditor?
    @AppStorage(ControlMethod.storageKey) private var defaultMethod = ControlMethod.automatic
    /// Whether the menu or the unlock screen has input rather than the game. Nil until the view first takes input.
    @State private var navigating: Bool?
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
            if let overlayMode {
                TouchControlsOverlay(mode: overlayMode, mapping: mapping, input: hub.input)
            }
            #endif
            if menu.isOpen {
                PlayerMenuView(menu: menu, title: title, method: method, showsFocus: hub.input.lastInputKind != .touch,
                               onResume: resume, onQuit: hub.endSession, onMethod: setMethod, onMapping: openMapping)
                    .transition(.opacity)
            } else if overlayMode == nil {
                // The touch overlays have their own Home button. Without one, this small button is how touch and pointer
                // users get back to the menu.
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
        .sheet(item: $mappingEditor) { editor in
            ButtonMappingSheet(editor: editor) { mappingEditor = nil }
                .environment(hub)
                .environment(nav)
        }
        .onAppear {
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

    private var method: ControlMethod { game?.controlMethod ?? defaultMethod }

    private var mapping: ButtonMapping { ButtonMapping(game?.buttonMapping) }

    /// The touch controls to show, if any. Macs have no touch screen, so they never show any.
    private var overlayMode: TouchControlsMode? {
        #if os(iOS)
        guard !menu.isOpen, !isCovered else { return nil }
        return method.overlay(lastInput: hub.input.lastInputKind, controllerConnected: !hub.input.controllerNames.isEmpty)
        #else
        return nil
        #endif
    }

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

    /// The game's input consumer. The VNDegen menu button never reaches the game.
    private func play(_ event: ControllerEvent) {
        if case let .button(.hub, pressed) = event {
            if pressed { openMenu() }
        } else {
            session.send(event)
        }
    }

    /// The input consumer while the menu or the unlock screen is up.
    private func navigate(_ event: ControllerEvent) {
        if isCovered {
            unlockView.handle(event)
            return
        }
        guard case let .button(button, pressed: true) = event else { return }
        if let mappingEditor {
            if !mappingEditor.handle(button), button == .cancel || button == .hub { self.mappingEditor = nil }
            return
        }
        let items = PlayerMenu.items
        let index = items.firstIndex(of: menu.focus) ?? 0
        switch button {
        case .up: menu.focus = items[max(0, index - 1)]
        case .down: menu.focus = items[min(items.count - 1, index + 1)]
        case .left, .right: if menu.focus == .method { setMethod(method.stepped(by: button == .left ? -1 : 1)) }
        case .confirm:
            switch menu.focus {
            case .resume: resume()
            case .quit: hub.endSession()
            case .method: setMethod(method.stepped(by: 1))
            case .mapping: openMapping()
            }
        case .cancel, .hub, .menu: resume()
        default: break
        }
    }

    /// Takes input from the game before pausing it, so the game handles the releases of the buttons it holds.
    private func openMenu() {
        menu.focus = .resume
        menu.isOpen = true
        applyInputMode()
        session.setPaused(true)
    }

    private func resume() {
        mappingEditor = nil
        menu.isOpen = false
        applyInputMode()
        session.setPaused(false)
    }

    /// Saved on the game, so the method sticks the next time it plays too.
    private func setMethod(_ method: ControlMethod) {
        guard let id = game?.id else { return }
        hub.library.update(id) { $0.controlMethod = method }
    }

    private func openMapping() {
        guard let id = game?.id else { return }
        mappingEditor = ButtonMappingEditor(library: hub.library, gameID: id)
    }

    /// The game reads the keyboard itself and wants raw presses through its own button mapping. The menu and the unlock
    /// screen want translated, repeating navigation in the standard layout. Each switch replaces the input consumer, which
    /// releases everything the outgoing one holds and keeps buttons still held silent until they are pressed again.
    private func applyInputMode() {
        let navigating = menu.isOpen || isCovered
        guard navigating != self.navigating else { return }
        self.navigating = navigating
        hub.input.onEvent = navigating ? navigate : play
        hub.input.translatesKeyboard = navigating
        hub.input.repeatsDirections = navigating
        hub.input.buttonMapping = navigating ? ButtonMapping() : mapping
    }
}

@MainActor
@Observable
final class PlayerMenu {
    enum Item { case resume, quit, method, mapping }
    /// Top to bottom. The control method only matters on a touch device.
    static let items: [Item] = Platform.current == .iOS ? [.resume, .quit, .method, .mapping] : [.resume, .quit, .mapping]
    var isOpen = false
    var focus = Item.resume
}

private struct PlayerMenuView: View {
    @Environment(\.hubMetrics) private var metrics
    let menu: PlayerMenu
    let title: String
    let method: ControlMethod
    let showsFocus: Bool
    let onResume: () -> Void
    let onQuit: () -> Void
    let onMethod: (ControlMethod) -> Void
    let onMapping: () -> Void

    var body: some View {
        ZStack {
            Rectangle().fill(.black.opacity(0.55)).ignoresSafeArea()
                .onTapGesture(perform: onResume)
            // Side by side when a landscape phone or a short window has no room for one column. Not ViewThatFits:
            // after a tap on Quit inside its second choice, SwiftUI kept the player in place, so the next game reused
            // this view and never started.
            let layout = metrics.isShort ? AnyLayout(HStackLayout(alignment: .center, spacing: 28)) : AnyLayout(VStackLayout(spacing: 18))
            layout {
                session
                controls
            }
            .padding(28)
            .background(.ultraThinMaterial, in: .rect(cornerRadius: 24, style: .continuous))
            .overlay { RoundedRectangle(cornerRadius: 24, style: .continuous).strokeBorder(Theme.stroke) }
        }
    }

    private var session: some View {
        VStack(spacing: 14) {
            Wordmark().padding(.bottom, 4)
            Text(title)
                .font(.system(size: 15, weight: .semibold))
                .foregroundStyle(Theme.secondaryText)
                .lineLimit(1)
            Button(action: onResume) { Label("Resume", systemImage: "play.fill").frame(minWidth: 220) }
                .buttonStyle(HubButtonStyle(kind: .primary, focused: isFocused(.resume)))
                .onHover { if $0 { menu.focus = .resume } }
            Button(action: onQuit) { Label("Quit to library", systemImage: "xmark").frame(minWidth: 220) }
                .buttonStyle(HubButtonStyle(kind: .secondary, focused: isFocused(.quit)))
                .onHover { if $0 { menu.focus = .quit } }
        }
    }

    private var controls: some View {
        VStack(spacing: 12) {
            SectionLabel(text: "Controls", systemImage: "gamecontroller")
            if PlayerMenu.items.contains(.method) { methods }
            Button(action: onMapping) { Label("Button mapping", systemImage: "slider.horizontal.3").frame(minWidth: 220) }
                .buttonStyle(HubButtonStyle(kind: .secondary, focused: isFocused(.mapping)))
                .onHover { if $0 { menu.focus = .mapping } }
        }
    }

    /// One focus target for the whole row. Left and right switch the method, like tabs.
    private var methods: some View {
        let focused = isFocused(.method)
        return HStack(spacing: 4) {
            ForEach(ControlMethod.allCases, id: \.self) { option in
                Button {
                    menu.focus = .method
                    onMethod(option)
                } label: {
                    Text(option.shortTitle)
                        .font(.system(size: 13, weight: .semibold))
                        .lineLimit(1)
                        .fixedSize()
                        .padding(.horizontal, 12)
                        .frame(minHeight: 34)
                        .background { Capsule().fill(option == method ? AnyShapeStyle(Theme.accent) : AnyShapeStyle(.clear)) }
                        .contentShape(.capsule)
                }
                .buttonStyle(.plain)
                .accessibilityLabel(option.title)
                .accessibilityAddTraits(option == method ? .isSelected : [])
            }
        }
        .padding(4)
        .background(Theme.surface, in: .capsule)
        .overlay { Capsule().strokeBorder(focused ? .white : Theme.stroke, lineWidth: focused ? 2.5 : 1) }
        .scaleEffect(focused ? 1.04 : 1)
        .animation(Theme.focusAnimation, value: focused)
        .onHover { if $0 { menu.focus = .method } }
    }

    private func isFocused(_ item: PlayerMenu.Item) -> Bool { showsFocus && menu.focus == item }
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
