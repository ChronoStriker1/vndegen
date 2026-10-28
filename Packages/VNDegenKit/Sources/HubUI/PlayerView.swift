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
    @Environment(\.hubMetrics) private var metrics

    var body: some View {
        ZStack {
            SessionViewHost(session: session)
                .ignoresSafeArea()
            if !menu.isOpen, !isCovered, !session.drawsOwnMenu {
                PointerView(mouse: hub.input.mouse, drawsArrow: overlayMode != .cursor)
                    .ignoresSafeArea()
            }
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
            if overlayMode == .off {
                // Swallows touches above the game. Disabling interaction on the game view would cut off its keyboard too.
                Color.clear
                    .contentShape(.rect)
                    .gesture(DragGesture(minimumDistance: 0))
                    .ignoresSafeArea()
            }
            if let overlayMode, overlayMode.hasOverlay {
                // In a short iPad window the gamepad's top row would reach the window's controls. The cursor's pad maps
                // to the whole game view, so it keeps the full height, and its Home button sits in the other corner.
                TouchControlsOverlay(mode: overlayMode, mapping: mapping, input: hub.input)
                    .padding(.top, overlayMode == .gamepad ? metrics.cornerClearance : 0)
            }
            if hub.input.mouse.showsKeyboard, !menu.isOpen, !isCovered {
                KeyboardBridge(session: session) { hub.input.mouse.hideKeyboard() }
                    .frame(width: 1, height: 1)
                    .allowsHitTesting(false)
                    .accessibilityHidden(true)
            }
            #endif
            if menu.isOpen {
                PlayerMenuView(menu: menu, title: title, method: method, showsFocus: hub.input.lastInputKind != .touch,
                               onResume: resume, onQuit: hub.endSession, onMethod: setMethod, onMapping: openMapping)
                    .transition(.opacity)
            } else if overlayMode?.hasOverlay != true {
                // The touch overlays have their own Home button. Without one, this small button is how touch and pointer
                // users get back to the menu.
                Button(action: menuButton) {
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
                .padding(.trailing, metrics.topClearance(.trailing))
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
        // The game and the touch gamepad get touches near the screen edges at once. iOS otherwise holds them while its own
        // edge gestures, such as the Home indicator's, decide.
        #if os(iOS)
        .defersSystemGestures(on: .all)
        #endif
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
            hub.input.mouse.start()
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
        // The game's view takes the keyboard back from the on-screen keyboard, so a hardware keyboard reaches it again.
        .onChange(of: hub.input.mouse.showsKeyboard) { _, shows in
            if !shows, !menu.isOpen { session.view.becomeFirstResponder() }
        }
        #endif
    }

    private var game: Game? {
        guard case let .playing(id, _) = hub.screen else { return nil }
        return hub.library.game(id)
    }

    private var orientation: PlayOrientation { game?.orientation ?? PlayOrientation.setting }

    private var method: ControlMethod { game?.controlMethod ?? defaultMethod }

    private var mapping: ButtonMapping { game.map { ButtonMapping(for: $0, in: hub.library) } ?? ButtonMapping() }

    /// What touches do in the game. Nil while the menu or the unlock screen covers it, and on a Mac, which has no touch screen.
    private var overlayMode: TouchControlsMode? {
        #if os(iOS)
        guard !menu.isOpen, !isCovered, !session.drawsOwnMenu else { return nil }
        return method.touchControls(lastInput: hub.input.lastInputKind, controllerConnected: !hub.input.controllerNames.isEmpty)
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
        let items = PlayerMenu.items(for: method)
        let index = items.firstIndex(of: menu.focus) ?? 0
        switch button {
        case .up: menu.focus = items[max(0, index - 1)]
        case .down: menu.focus = items[min(items.count - 1, index + 1)]
        case .left, .right:
            switch menu.focus {
            case .method: setMethod(method.stepped(by: button == .left ? -1 : 1))
            case .mouseStyle: setMethod(method.switchingMouseStyle)
            case .resume, .quit, .controllerMouse, .mapping: break
            }
        case .confirm:
            switch menu.focus {
            case .resume: resume()
            case .quit: hub.endSession()
            case .method: setMethod(method.stepped(by: 1))
            case .controllerMouse: setMethod(method.togglingMouse)
            case .mouseStyle: setMethod(method.switchingMouseStyle)
            case .mapping: openMapping()
            }
        case .cancel, .hub, .menu: resume()
        default: break
        }
    }

    private func menuButton() {
        if session.drawsOwnMenu {
            session.send(.button(.hub, pressed: true))
            session.send(.button(.hub, pressed: false))
        } else {
            openMenu()
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
    /// A menu row the new method hides hands focus to the methods row.
    private func setMethod(_ method: ControlMethod) {
        guard let id = game?.id else { return }
        hub.library.update(id) { $0.controlMethod = method }
        if !PlayerMenu.items(for: method).contains(menu.focus) { menu.focus = .method }
    }

    private func openMapping() {
        guard let id = game?.id else { return }
        mappingEditor = ButtonMappingEditor(library: hub.library, gameID: id)
    }

    /// The game reads the keyboard itself and wants raw presses through its own button mapping. The menu and the unlock
    /// screen want translated, repeating navigation in the hub's own layout. Each switch replaces the input consumer, which
    /// releases everything the outgoing one holds and keeps buttons still held silent until they are pressed again. A
    /// game that reads controllers itself gets none from the hub, so no press reaches it twice.
    private func applyInputMode() {
        let navigating = menu.isOpen || isCovered
        guard navigating != self.navigating else { return }
        self.navigating = navigating
        hub.input.onEvent = navigating ? navigate : session.drawsOwnMenu ? nil : play
        hub.input.translatesKeyboard = navigating
        hub.input.repeatsDirections = navigating
        hub.input.buttonMapping = navigating ? nil : mapping
    }
}

@MainActor
@Observable
final class PlayerMenu {
    enum Item { case resume, quit, method, controllerMouse, mouseStyle, mapping }

    /// Top to bottom. The control method only matters on a touch device.
    static func items(for method: ControlMethod) -> [Item] {
        guard Platform.current == .iOS else { return [.resume, .quit, .mapping] }
        var items: [Item] = [.resume, .quit, .method]
        if method.kind == .controller { items.append(.controllerMouse) }
        if method.mouse != nil { items.append(.mouseStyle) }
        return items + [.mapping]
    }

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
            // Side by side when a landscape phone or a short window has no room for one column, and is wide enough for
            // two: the columns' 220 pt buttons, spacing and padding need about 620 pt. Not ViewThatFits: after a tap on
            // Quit inside its second choice, SwiftUI kept the player in place, so the next game reused this view and
            // never started.
            let sideBySide = metrics.isShort && metrics.size.width >= 640
            let layout = sideBySide ? AnyLayout(HStackLayout(alignment: .center, spacing: 28)) : AnyLayout(VStackLayout(spacing: 18))
            layout {
                session
                controls
            }
            .padding(isNarrow ? 20 : 28)
            .background(.ultraThinMaterial, in: .rect(cornerRadius: 24, style: .continuous))
            .overlay { RoundedRectangle(cornerRadius: 24, style: .continuous).strokeBorder(Theme.stroke) }
            .padding(12)
        }
    }

    private var session: some View {
        VStack(spacing: 14) {
            Wordmark().padding(.bottom, 4)
            Text(title)
                .font(.system(size: 15, weight: .semibold))
                .foregroundStyle(Theme.secondaryText)
                .lineLimit(1)
            Button(action: onResume) { Label("Resume", systemImage: "play.fill").frame(minWidth: buttonWidth) }
                .buttonStyle(HubButtonStyle(kind: .primary, focused: isFocused(.resume)))
                .onHover { if $0 { menu.focus = .resume } }
            Button(action: onQuit) { Label("Quit to library", systemImage: "xmark").frame(minWidth: buttonWidth) }
                .buttonStyle(HubButtonStyle(kind: .secondary, focused: isFocused(.quit)))
                .onHover { if $0 { menu.focus = .quit } }
        }
    }

    private var controls: some View {
        VStack(spacing: 12) {
            SectionLabel(text: "Controls", systemImage: "gamecontroller")
            let items = PlayerMenu.items(for: method)
            if items.contains(.method) { methods }
            if items.contains(.controllerMouse) { controllerMouse }
            if items.contains(.mouseStyle), let style = method.mouse { mouseStyles(style) }
            Button(action: onMapping) { Label("Button mapping", systemImage: "slider.horizontal.3").frame(minWidth: buttonWidth) }
                .buttonStyle(HubButtonStyle(kind: .secondary, focused: isFocused(.mapping)))
                .onHover { if $0 { menu.focus = .mapping } }
        }
    }

    /// Below about 400 pt, the width of an iPhone in portrait, the four methods don't fit in one row, so they take two.
    private var isNarrow: Bool { metrics.size.width < 400 }

    /// The buttons' label width. In a narrow window, room for the button style's 56 pt, the card's padding, and its margin.
    private var buttonWidth: CGFloat { isNarrow ? max(0, min(220, metrics.size.width - 56 - 40 - 24)) : 220 }

    /// One focus target for the whole row. Left and right switch the method, like tabs.
    private var methods: some View {
        let kinds = ControlMethod.Kind.allCases
        let rows = isNarrow ? [Array(kinds.prefix(2)), Array(kinds.dropFirst(2))] : [Array(kinds)]
        return segments(.method, rows: rows.count) {
            VStack(spacing: 4) {
                ForEach(rows, id: \.self) { row in
                    HStack(spacing: 4) {
                        ForEach(row, id: \.self) { kind in
                            segment(kind.shortTitle, selected: kind == method.kind, item: .method) { onMethod(ControlMethod(kind)) }
                                .accessibilityLabel(kind.title)
                        }
                    }
                }
            }
        }
    }

    private var controllerMouse: some View {
        segments(.controllerMouse) {
            Toggle("Mouse", isOn: Binding(get: { method.mouse != nil }, set: { _ in
                menu.focus = .controllerMouse
                onMethod(method.togglingMouse)
            }))
            .toggleStyle(CheckboxSegmentStyle())
        }
    }

    private func mouseStyles(_ current: MouseStyle) -> some View {
        segments(.mouseStyle) {
            ForEach(MouseStyle.allCases, id: \.self) { style in
                segment(style.title, selected: style == current, item: .mouseStyle) { onMethod(method.with(mouse: style)) }
            }
        }
    }

    /// A capsule of segments that takes focus as one item. Two rows round their corners as one row's ends are.
    private func segments(_ item: PlayerMenu.Item, rows: Int = 1, @ViewBuilder content: () -> some View) -> some View {
        let focused = isFocused(item)
        let shape = RoundedRectangle(cornerRadius: rows > 1 ? 21 : .infinity, style: .continuous)
        return HStack(spacing: 4) { content() }
            .padding(4)
            .background(Theme.surface, in: shape)
            .overlay { shape.strokeBorder(focused ? .white : Theme.stroke, lineWidth: focused ? 2.5 : 1) }
            .scaleEffect(focused ? 1.04 : 1)
            .animation(Theme.focusAnimation, value: focused)
            .onHover { if $0 { menu.focus = item } }
    }

    private func segment(_ title: String, selected: Bool, item: PlayerMenu.Item, action: @escaping () -> Void) -> some View {
        Button {
            menu.focus = item
            action()
        } label: {
            SegmentLabel(title: title, selected: selected)
        }
        .buttonStyle(.plain)
        .accessibilityAddTraits(selected ? .isSelected : [])
    }

    private func isFocused(_ item: PlayerMenu.Item) -> Bool { showsFocus && menu.focus == item }
}

private struct SegmentLabel: View {
    let title: String
    let selected: Bool

    var body: some View {
        Text(title)
            .font(.system(size: 13, weight: .semibold))
            .lineLimit(1)
            .fixedSize()
            .padding(.horizontal, 12)
            .frame(minHeight: 34)
            .background { Capsule().fill(selected ? AnyShapeStyle(Theme.accent) : AnyShapeStyle(.clear)) }
            .contentShape(.capsule)
    }
}

/// A checkbox the size of a menu segment.
private struct CheckboxSegmentStyle: ToggleStyle {
    func makeBody(configuration: Configuration) -> some View {
        Button { configuration.isOn.toggle() } label: {
            HStack(spacing: 6) {
                Image(systemName: configuration.isOn ? "checkmark.square.fill" : "square")
                    .font(.system(size: 15, weight: .semibold))
                    .foregroundStyle(configuration.isOn ? Theme.accent : .white)
                configuration.label
                    .font(.system(size: 13, weight: .semibold))
                    .lineLimit(1)
                    .fixedSize()
            }
            .padding(.horizontal, 12)
            .frame(minHeight: 34)
            .contentShape(.capsule)
        }
        .buttonStyle(.plain)
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
