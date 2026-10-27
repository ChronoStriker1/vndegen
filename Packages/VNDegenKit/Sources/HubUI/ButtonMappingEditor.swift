import SwiftUI
import VNCore
import GameInput

extension ControlMethod {
    /// The Settings value. Automatic until the user changes it.
    static var setting: ControlMethod {
        UserDefaults.standard.string(forKey: storageKey).flatMap(Self.init(rawValue:)) ?? .automatic
    }

    /// Whether a game's control method changes anything: only on a touch device, and only when the game's engine
    /// leaves touches to VNDegen.
    static func applies(to engine: (any EnginePlugin)?, on platform: Platform = .current) -> Bool {
        platform == .iOS && engine?.readsTouches != true
    }

    /// Settings and the in-game menu pick one of these first, then a controller's mouse and the mouse style under it.
    enum Kind: CaseIterable {
        case automatic, gamepad, mouse, controller

        var title: String {
            switch self {
            case .automatic: "Automatic"
            case .gamepad: "Touch gamepad"
            case .mouse: "Mouse"
            case .controller: "Controller"
            }
        }

        /// For the in-game menu's row of methods.
        var shortTitle: String {
            switch self {
            case .automatic: "Auto"
            case .gamepad: "Gamepad"
            case .mouse: "Mouse"
            case .controller: "Controller"
            }
        }
    }

    /// What picking a kind selects. A mouse starts with touch, and a controller starts without a mouse.
    init(_ kind: Kind) {
        self = switch kind {
        case .automatic: .automatic
        case .gamepad: .gamepad
        case .mouse: .mouse(.touch)
        case .controller: .controller(mouse: nil)
        }
    }

    var kind: Kind {
        switch self {
        case .automatic: .automatic
        case .gamepad: .gamepad
        case .mouse: .mouse
        case .controller: .controller
        }
    }

    var mouse: MouseStyle? {
        switch self {
        case let .mouse(style): style
        case let .controller(mouse): mouse
        case .automatic, .gamepad: nil
        }
    }

    /// Turns a controller's mouse on with touch, or off.
    var togglingMouse: ControlMethod {
        guard case let .controller(mouse) = self else { return self }
        return .controller(mouse: mouse == nil ? .touch : nil)
    }

    /// Switches between touch and the cursor, for a mouse or a controller with one.
    var switchingMouseStyle: ControlMethod { mouse.map { with(mouse: $0.other) } ?? self }

    func with(mouse style: MouseStyle) -> ControlMethod {
        switch self {
        case .mouse: .mouse(style)
        case .controller: .controller(mouse: style)
        case .automatic, .gamepad: self
        }
    }

    var title: String {
        switch self {
        case .automatic: "Automatic"
        case .gamepad: "Touch gamepad"
        case .mouse(.touch): "Mouse, touch"
        case .mouse(.cursor): "Mouse, cursor"
        case .controller(mouse: nil): "Controller only"
        case .controller(mouse: .touch): "Controller and touch"
        case .controller(mouse: .cursor): "Controller and cursor"
        }
    }

    var detail: String {
        switch self {
        case .automatic: "The touch gamepad while you play by touch with no controller connected"
        case .gamepad: "Always show the on-screen gamepad"
        case .mouse(.touch): "Tap or drag right where you want, as on a touch screen"
        case .mouse(.cursor): "Drag to move a cursor, tap to click, tap with two fingers to right click"
        case .controller(mouse: nil): "No on-screen controls, and the game ignores touches"
        case .controller(mouse: .touch): "No on-screen controls, and touches work where you tap"
        case .controller(mouse: .cursor): "No on-screen controls, and dragging moves a cursor"
        }
    }

    /// Steps through the kinds, like tabs.
    func stepped(by step: Int) -> ControlMethod {
        let all = Kind.allCases
        let index = all.firstIndex(of: kind) ?? 0
        return ControlMethod(all[(index + step + all.count) % all.count])
    }
}

extension MouseStyle {
    var title: String {
        switch self {
        case .touch: "Touch"
        case .cursor: "Cursor"
        }
    }

    var other: MouseStyle { self == .touch ? .cursor : .touch }
}

/// The button mapping editor's state: pick a button, then pick what it does. Touch, pointer, and controller drive the
/// same state, in the hub's game page and in a running game's menu. Changes save to the game as they are made.
@MainActor
@Observable
final class ButtonMappingEditor: Identifiable {
    enum Target: Hashable {
        case input(ControlInput)
        /// Applies a whole layout, such as `ButtonMapping.rpgMaker`.
        case apply(Layout)
        /// Puts back the layout the game's engine starts with.
        case reset
        /// Turns a whole stick into mouse movement, or back into a stick.
        case stick(Stick)
        case action(ControlAction)
    }

    /// The layouts the top of the button list applies.
    enum Layout: CaseIterable {
        case keyboardAndMouse, mouse, rpgMaker, gamepad

        var mapping: ButtonMapping {
            switch self {
            case .keyboardAndMouse: .keyboardAndMouse
            case .mouse: .mouse
            case .rpgMaker: .rpgMaker
            case .gamepad: ButtonMapping()
            }
        }

        var title: String {
            switch self {
            case .keyboardAndMouse: "Keyboard and mouse"
            case .mouse: "Mouse"
            case .rpgMaker: "RPG Maker controls"
            case .gamepad: "Xbox gamepad"
            }
        }

        var systemImage: String {
            switch self {
            case .keyboardAndMouse: "desktopcomputer"
            case .mouse: "computermouse"
            case .rpgMaker: "keyboard"
            case .gamepad: "gamecontroller"
            }
        }

        /// The layout's name for a game's button mapping, or nil when it is none of them.
        static func named(_ mapping: ButtonMapping) -> Layout? {
            allCases.first { $0.mapping == mapping }
        }
    }

    struct Section {
        let title: String
        let actions: [ControlAction]
        /// The narrowest a choice gets, which sets how many fit on a row.
        let minWidth: CGFloat
    }

    static let sections = [
        Section(title: "Gamepad", actions: ControlAction.gamepadChoices, minWidth: 120),
        Section(title: "Keyboard", actions: ControlAction.keyChoices, minWidth: 54),
        Section(title: "Mouse", actions: ControlAction.mouseChoices, minWidth: 140),
        Section(title: "Other", actions: [.showKeyboard, .nothing], minWidth: 140),
    ]
    static let spacing: CGFloat = 8

    let gameID: GameID
    private let library: LibraryStore
    /// The button whose action is being picked. Nil while the list of buttons shows.
    private(set) var picking: ControlInput?
    private var target: Target?
    /// The focus ring shows once a controller or the keyboard moved focus, so touch users never see it.
    private(set) var showsFocus = false
    /// Set by the sheet from its width, so the focus rows match the grids it draws.
    var width: CGFloat = 360

    init(library: LibraryStore, gameID: GameID) {
        self.library = library
        self.gameID = gameID
    }

    var mapping: ButtonMapping { ButtonMapping(stored: library.game(gameID)?.buttonMapping, default: defaultLayout) }

    /// What the game's engine starts its games with.
    var defaultLayout: ButtonMapping {
        library.game(gameID).map { ButtonMapping.defaultLayout(for: $0, in: library) } ?? ButtonMapping()
    }

    /// The button list's rows below the layouts: every button, with a row for each whole stick above its directions.
    static let inputRows: [Target] = ControlInput.allCases.flatMap { input -> [Target] in
        let stick = Stick.allCases.first { $0.directions.first == input }
        return (stick.map { [Target.stick($0)] } ?? []) + [.input(input)]
    }

    func columns(_ section: Section) -> Int {
        columns(minWidth: section.minWidth)
    }

    /// The layout buttons sit in a grid as wide as the sheet.
    var layoutColumns: Int { columns(minWidth: 180) }

    private func columns(minWidth: CGFloat) -> Int {
        max(1, Int((width + Self.spacing) / (minWidth + Self.spacing)))
    }

    func rows() -> [[Target]] {
        groups().flatMap(\.self)
    }

    /// The rows in groups that the shoulder buttons jump between: the layouts and the buttons, or each kind of action.
    private func groups() -> [[[Target]]] {
        guard picking != nil else {
            return [Self.chunked(Layout.allCases.map(Target.apply), layoutColumns) + [[.reset]], Self.inputRows.map { [$0] }]
        }
        return Self.sections.map { Self.chunked($0.actions.map(Target.action), columns($0)) }
    }

    private static func chunked(_ targets: [Target], _ columns: Int) -> [[Target]] {
        stride(from: 0, to: targets.count, by: columns).map { Array(targets[$0..<min($0 + columns, targets.count)]) }
    }

    /// The focused target, or the default when the stored one is not on screen.
    var focus: Target? {
        let rows = rows()
        let fallback: Target = picking.map { .action(mapping.action(for: $0)) } ?? .input(ControlInput.allCases[0])
        for candidate in [target, fallback] {
            if let candidate, rows.contains(where: { $0.contains(candidate) }) { return candidate }
        }
        return rows.first?.first
    }

    func isFocused(_ candidate: Target) -> Bool { showsFocus && focus == candidate }

    /// Directions move focus, confirm activates it, and cancel leaves the action list. Returns false for buttons it does
    /// not use, so the caller can close the editor on cancel from the button list.
    func handle(_ button: VirtualButton) -> Bool {
        switch button {
        case .up, .down, .left, .right:
            // The first press only reveals the ring where focus already is.
            if showsFocus { move(button) }
            showsFocus = true
            return true
        case .confirm:
            showsFocus = true
            if let focus { activate(focus) }
            return true
        case .cancel:
            guard picking != nil else { return false }
            closePicker()
            return true
        case .pageUp, .pageDown:
            showsFocus = true
            jumpGroup(by: button == .pageUp ? -1 : 1)
            return true
        default:
            return false
        }
    }

    /// Moves focus to the first target of the previous or next group, such as from the gamepad buttons to the keys.
    private func jumpGroup(by step: Int) {
        let groups = groups()
        guard let focus, let index = groups.firstIndex(where: { $0.contains { $0.contains(focus) } }) else { return }
        let next = min(max(0, index + step), groups.count - 1)
        target = groups[next].first?.first
    }

    /// Touch and pointer: activate without showing the ring.
    func select(_ candidate: Target) {
        target = candidate
        showsFocus = false
        activate(candidate)
    }

    func closePicker() {
        target = picking.map(Target.input)
        picking = nil
    }

    private func move(_ button: VirtualButton) {
        let rows = rows()
        guard let focus, let r = rows.firstIndex(where: { $0.contains(focus) }), let c = rows[r].firstIndex(of: focus) else { return }
        switch button {
        case .up where r > 0: target = rows[r - 1][min(c, rows[r - 1].count - 1)]
        case .down where r < rows.count - 1: target = rows[r + 1][min(c, rows[r + 1].count - 1)]
        case .left: target = rows[r][max(0, c - 1)]
        case .right: target = rows[r][min(rows[r].count - 1, c + 1)]
        default: target = focus
        }
    }

    private func save(_ mapping: ButtonMapping) {
        let stored = mapping.stored(default: defaultLayout)
        library.update(gameID) { $0.buttonMapping = stored }
    }

    private func activate(_ candidate: Target) {
        switch candidate {
        case let .input(input):
            picking = input
            target = nil
        case let .apply(layout):
            save(layout.mapping)
        case .reset:
            save(defaultLayout)
        case let .stick(stick):
            var mapping = mapping
            mapping.setStick(stick, movesPointer: !mapping.movesPointer(stick))
            save(mapping)
        case let .action(action):
            guard let input = picking else { return }
            var mapping = mapping
            mapping.set(action, for: input)
            save(mapping)
            closePicker()
        }
    }
}

/// Lists the buttons with what each does, and picks a new action for one.
struct ButtonMappingSheet: View {
    @Environment(Hub.self) private var hub
    let editor: ButtonMappingEditor
    /// Defaults to closing the navigator's sheet.
    var onClose: (() -> Void)?

    var body: some View {
        let title = hub.library.game(editor.gameID).map(hub.displayTitle) ?? ""
        SheetScaffold(title: editor.picking.map { "Map \($0.title)" } ?? "Button mapping", subtitle: title, onClose: onClose) {
            ScrollViewReader { proxy in
                ScrollView {
                    Group {
                        if let input = editor.picking { picker(for: input) } else { inputList }
                    }
                    .padding(.horizontal, 22)
                    .padding(.bottom, 22)
                }
                .onChange(of: editor.focus) { _, focus in
                    guard editor.showsFocus, let focus else { return }
                    withAnimation(Theme.focusAnimation) { proxy.scrollTo(focus, anchor: .center) }
                }
            }
            .onGeometryChange(for: CGFloat.self) { $0.size.width - 44 } action: { editor.width = $0 }
        }
        .presentationDetents([.large])
        #if os(macOS)
        .frame(minWidth: 520, minHeight: 560)
        #endif
    }

    private var inputList: some View {
        let mapping = editor.mapping, defaultLayout = editor.defaultLayout
        return VStack(alignment: .leading, spacing: 8) {
            LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: ButtonMappingEditor.spacing),
                                     count: editor.layoutColumns),
                      spacing: ButtonMappingEditor.spacing) {
                ForEach(ButtonMappingEditor.Layout.allCases, id: \.self) { layout in
                    let target = ButtonMappingEditor.Target.apply(layout)
                    Button { editor.select(target) } label: {
                        Label(layout.title, systemImage: layout.systemImage)
                            .lineLimit(1)
                            .minimumScaleFactor(0.7)
                            .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(HubButtonStyle(kind: .secondary, focused: editor.isFocused(target)))
                    .disabled(mapping == layout.mapping)
                    .id(target)
                }
            }
            Button { editor.select(.reset) } label: {
                Label("Reset to \(ButtonMappingEditor.Layout.named(defaultLayout)?.title ?? "the default")", systemImage: "arrow.counterclockwise")
                    .lineLimit(1)
                    .minimumScaleFactor(0.7)
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(HubButtonStyle(kind: .secondary, focused: editor.isFocused(.reset)))
            .disabled(mapping == defaultLayout)
            .id(ButtonMappingEditor.Target.reset)
            .padding(.bottom, 8)
            ForEach(ButtonMappingEditor.inputRows, id: \.self) { target in
                switch target {
                case let .input(input):
                    row(target, name: input.title, action: mapping.action(for: input).title,
                        controllersOnly: !input.isOnTouchGamepad,
                        changed: mapping.action(for: input) != defaultLayout.action(for: input))
                case let .stick(stick):
                    let isStick = stick.directions.allSatisfy { mapping.action(for: $0) == .gamepad($0) }
                    row(target, name: stick.title,
                        action: mapping.movesPointer(stick) ? "Mouse movement" : isStick ? "Stick" : "Set by direction",
                        controllersOnly: true, detail: mapping.movesPointer(stick) ? "Use as a stick" : "Use as mouse movement")
                default:
                    EmptyView()
                }
            }
            Text(footnote(defaultLayout))
                .font(.system(size: 13))
                .foregroundStyle(Theme.tertiaryText)
                .fixedSize(horizontal: false, vertical: true)
                .padding(.top, 14)
        }
        .animation(Theme.focusAnimation, value: editor.focus)
    }

    private func footnote(_ defaultLayout: ButtonMapping) -> String {
        let start = defaultLayout == .keyboardAndMouse
            ? "This game's engine reads no gamepad, so its games start with Keyboard and mouse, the layout of Steam's template of that name: the left stick is WASD, the right stick moves the pointer, RT and R3 click, LT right clicks, LB and RB turn the wheel, and the other buttons press keys. Mouse puts the pointer on both sticks and the mouse buttons on A and X."
            : "Every button sends the same Xbox controller button unless you change it, and the game reads it as a gamepad."
        return start + " Home on a controller, the house button on the touch gamepad, and the backtick key always open the VNDegen menu. The library and the VNDegen menu keep their own navigation, so a change here never stops a controller from working them. LB and RB jump between groups."
    }

    /// A button, or a whole stick, with what it does. A stick's row switches it between moving the pointer and being a
    /// stick, and says which it would become.
    private func row(_ target: ButtonMappingEditor.Target, name: String, action: String, controllersOnly: Bool,
                     changed: Bool = false, detail: String? = nil) -> some View {
        Button { editor.select(target) } label: {
            HStack(spacing: 14) {
                // One width for every button, so the actions line up.
                Text(name)
                    .font(.system(size: 14, weight: .heavy, design: .rounded))
                    .lineLimit(1)
                    .minimumScaleFactor(0.8)
                    .padding(.horizontal, 6)
                    .frame(width: 104, height: 32)
                    .background(Theme.surface, in: .capsule)
                    .overlay { Capsule().strokeBorder(Theme.stroke) }
                VStack(alignment: .leading, spacing: 2) {
                    Text(action).font(.system(size: 16, weight: .semibold))
                    if controllersOnly {
                        Text("Controllers only").font(.system(size: 13)).foregroundStyle(Theme.secondaryText)
                    }
                }
                Spacer()
                if changed { Tag(text: "Changed", prominent: true) }
                if let detail {
                    Text(detail).font(.system(size: 13, weight: .semibold)).foregroundStyle(Theme.secondaryText)
                    Image(systemName: "arrow.left.arrow.right").font(.footnote.weight(.semibold)).foregroundStyle(Theme.tertiaryText)
                } else {
                    Image(systemName: "chevron.right").font(.footnote.weight(.semibold)).foregroundStyle(Theme.tertiaryText)
                }
            }
            .padding(12)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(rowFill(target), in: .rect(cornerRadius: 14, style: .continuous))
            .overlay { rowStroke(target) }
            .contentShape(.rect)
        }
        .buttonStyle(.plain)
        .id(target)
    }

    private func picker(for input: ControlInput) -> some View {
        let current = editor.mapping.action(for: input)
        return VStack(alignment: .leading, spacing: 16) {
            Button { editor.closePicker() } label: { Label("All buttons", systemImage: "chevron.left") }
                .buttonStyle(HubButtonStyle(kind: .secondary, focused: false))
            ForEach(ButtonMappingEditor.sections, id: \.title) { section in
                VStack(alignment: .leading, spacing: 10) {
                    SectionLabel(text: section.title)
                    LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: ButtonMappingEditor.spacing),
                                             count: editor.columns(section)),
                              spacing: ButtonMappingEditor.spacing) {
                        ForEach(section.actions, id: \.self) { action in
                            choice(action, selected: action == current)
                        }
                    }
                }
            }
        }
        .animation(Theme.focusAnimation, value: editor.focus)
    }

    private func choice(_ action: ControlAction, selected: Bool) -> some View {
        let target = ButtonMappingEditor.Target.action(action)
        let focused = editor.isFocused(target)
        let label = if case let .key(key) = action { key.label } else { action.title }
        return Button { editor.select(target) } label: {
            Text(label)
                .font(.system(size: 15, weight: .semibold))
                .lineLimit(1)
                .minimumScaleFactor(0.7)
                .foregroundStyle(focused ? .black : .white)
                .frame(maxWidth: .infinity, minHeight: 44)
                .background {
                    let shape = RoundedRectangle(cornerRadius: 10, style: .continuous)
                    shape.fill(focused ? AnyShapeStyle(.white) : selected ? AnyShapeStyle(Theme.accent) : AnyShapeStyle(Theme.surface))
                }
                .overlay { RoundedRectangle(cornerRadius: 10, style: .continuous).strokeBorder(Theme.stroke) }
                .contentShape(.rect)
        }
        .buttonStyle(.plain)
        .accessibilityLabel(action.title)
        .accessibilityAddTraits(selected ? .isSelected : [])
        .id(target)
    }

    private func rowFill(_ target: ButtonMappingEditor.Target) -> Color {
        editor.isFocused(target) ? .white.opacity(0.16) : Theme.surface
    }

    private func rowStroke(_ target: ButtonMappingEditor.Target) -> some View {
        let focused = editor.isFocused(target)
        return RoundedRectangle(cornerRadius: 14, style: .continuous)
            .strokeBorder(focused ? .white : Theme.stroke, lineWidth: focused ? 2 : 1)
    }
}
