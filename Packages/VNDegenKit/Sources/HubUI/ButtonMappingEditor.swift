import SwiftUI
import VNCore
import GameInput

extension ControlMethod {
    static let storageKey = "controlMethod"

    /// The Settings value. Automatic until the user changes it.
    static var setting: ControlMethod {
        UserDefaults.standard.string(forKey: storageKey).flatMap(Self.init(rawValue:)) ?? .automatic
    }

    var title: String {
        switch self {
        case .automatic: "Automatic"
        case .touchGamepad: "Touch gamepad"
        case .touchMouse: "Touch mouse"
        case .controllerOnly: "Controller only"
        }
    }

    /// For the in-game menu's row of methods.
    var shortTitle: String {
        switch self {
        case .automatic: "Auto"
        case .touchGamepad: "Gamepad"
        case .touchMouse: "Mouse"
        case .controllerOnly: "Controller"
        }
    }

    var detail: String {
        switch self {
        case .automatic: "The touch gamepad while you play by touch with no controller connected"
        case .touchGamepad: "Always show the on-screen gamepad"
        case .touchMouse: "Drag to move a cursor, tap to click, tap with two fingers to right click"
        case .controllerOnly: "No on-screen controls, for a controller or a keyboard"
        }
    }

    func stepped(by step: Int) -> ControlMethod {
        let all = Self.allCases
        let index = all.firstIndex(of: self) ?? 0
        return all[(index + step + all.count) % all.count]
    }
}

/// The button mapping editor's state: pick a button, then pick what it does. Touch, pointer, and controller drive the
/// same state, in the hub's game page and in a running game's menu. Changes save to the game as they are made.
@MainActor
@Observable
final class ButtonMappingEditor: Identifiable {
    enum Target: Hashable {
        case input(ControlInput)
        case reset
        case action(ControlAction)
    }

    struct Section {
        let title: String
        let actions: [ControlAction]
        /// The narrowest a choice gets, which sets how many fit on a row.
        let minWidth: CGFloat
    }

    static let sections = [
        Section(title: "Gamepad", actions: ControlAction.gamepadChoices + [.nothing], minWidth: 120),
        Section(title: "Mouse", actions: ControlAction.mouseChoices, minWidth: 120),
        Section(title: "Keyboard", actions: ControlAction.keyChoices, minWidth: 54),
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

    var mapping: ButtonMapping { ButtonMapping(library.game(gameID)?.buttonMapping) }

    func columns(_ section: Section) -> Int {
        max(1, Int((width + Self.spacing) / (section.minWidth + Self.spacing)))
    }

    func rows() -> [[Target]] {
        guard picking != nil else { return ControlInput.allCases.map { [.input($0)] } + [[.reset]] }
        return Self.sections.flatMap { section in
            let targets = section.actions.map(Target.action)
            let columns = columns(section)
            return stride(from: 0, to: targets.count, by: columns).map { Array(targets[$0..<min($0 + columns, targets.count)]) }
        }
    }

    /// The focused target, or the default when the stored one is not on screen.
    var focus: Target? {
        let rows = rows()
        let fallback: Target = picking.map { .action(mapping.action(for: $0)) } ?? .input(.up)
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
        default:
            return false
        }
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

    private func activate(_ candidate: Target) {
        switch candidate {
        case let .input(input):
            picking = input
            target = nil
        case .reset:
            library.update(gameID) { $0.buttonMapping = nil }
        case let .action(action):
            guard let input = picking else { return }
            library.update(gameID) { game in
                var mapping = ButtonMapping(game.buttonMapping)
                mapping.set(action, for: input)
                game.buttonMapping = mapping.stored
            }
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
        let mapping = editor.mapping
        return VStack(alignment: .leading, spacing: 8) {
            ForEach(ControlInput.allCases, id: \.self) { input in
                let target = ButtonMappingEditor.Target.input(input)
                Button { editor.select(target) } label: {
                    HStack(spacing: 14) {
                        // One width for every button, so the actions line up.
                        Text(input.title)
                            .font(.system(size: 14, weight: .heavy, design: .rounded))
                            .lineLimit(1)
                            .minimumScaleFactor(0.8)
                            .padding(.horizontal, 6)
                            .frame(width: 104, height: 32)
                            .background(Theme.surface, in: .capsule)
                            .overlay { Capsule().strokeBorder(Theme.stroke) }
                        VStack(alignment: .leading, spacing: 2) {
                            Text(mapping.action(for: input).title).font(.system(size: 16, weight: .semibold))
                            if !input.isOnTouchGamepad {
                                Text("Controllers only").font(.system(size: 13)).foregroundStyle(Theme.secondaryText)
                            }
                        }
                        Spacer()
                        if mapping.isRemapped(input) { Tag(text: "Changed", prominent: true) }
                        Image(systemName: "chevron.right").font(.footnote.weight(.semibold)).foregroundStyle(Theme.tertiaryText)
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
            Button { editor.select(.reset) } label: {
                Label("Reset to defaults", systemImage: "arrow.counterclockwise")
            }
            .buttonStyle(HubButtonStyle(kind: .secondary, focused: editor.isFocused(.reset)))
            .disabled(mapping.overrides.isEmpty)
            .padding(.top, 8)
            .id(ButtonMappingEditor.Target.reset)
            Text("Options and Home on a controller always open the VNDegen menu, and so does the house button on the touch gamepad. The library and the VNDegen menu always use the standard layout.")
                .font(.system(size: 13))
                .foregroundStyle(Theme.tertiaryText)
                .fixedSize(horizontal: false, vertical: true)
                .padding(.top, 6)
        }
        .animation(Theme.focusAnimation, value: editor.focus)
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
