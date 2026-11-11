import SwiftUI
import VNCore
import ControllerUI
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

extension ControllerLayout.ID {
    var systemImage: String {
        switch self {
        case .visualNovel: "text.bubble"
        case .keyboardAndMouse: "desktopcomputer"
        case .mouse: "computermouse"
        case .rpgMaker: "keyboard"
        case .gamepad: "gamecontroller"
        case .saved: "person.crop.circle"
        case .suggested: "wand.and.stars"
        }
    }
}

/// The controller layout sheet's state: pick a layout, or pick a button and then what it does. Touch, pointer, and
/// controller drive the same state, in the hub's game page and in a running game's menu. Changes save as they are made,
/// to the game, or to the saved layout being edited and the games that show it.
@MainActor
@Observable
final class ButtonMappingEditor: Identifiable {
    enum Target: Hashable {
        case input(ControlInput)
        /// Opens or closes the list of layouts under the layout the game shows.
        case layouts
        /// Copies a layout's buttons into the game.
        case apply(ControllerLayout.ID)
        /// Saves the game's buttons as a new layout.
        case saveNew
        case reset
        /// Opens a saved layout, so changes go to it rather than the game.
        case edit(ControllerLayout.ID)
        case rename
        /// Asks before deleting the layout being edited.
        case delete
        case confirmDelete
        case cancelDelete
        /// Turns a whole stick into mouse movement, or back into a stick.
        case stick(Stick)
        case action(ControlAction)
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
    let layouts: LayoutLibrary
    /// The saved layout changes go to. Nil while they go to the game.
    private(set) var editing: ControllerLayout.ID?
    private(set) var confirmingDelete = false
    /// The list of layouts shows. It starts closed, so the buttons come first, and closes again once one is picked.
    private(set) var listsLayouts = false
    /// The keyboard a new layout's name is typed on. Finishing it saves the name.
    private(set) var keyboard: OnScreenKeyboard?
    /// The button whose action is being picked. Nil while the list of buttons shows.
    private(set) var picking: ControlInput?
    private var target: Target?
    /// The focus ring shows once a controller or the keyboard moved focus, so touch users never see it.
    private(set) var showsFocus = false
    /// Set by the sheet from its width, so the focus rows match the grids it draws.
    var width: CGFloat = 360

    init(library: LibraryStore, layouts: LayoutLibrary, gameID: GameID) {
        self.library = library
        self.layouts = layouts
        self.gameID = gameID
    }

    /// The buttons being edited: the saved layout's, or the game's.
    var mapping: ButtonMapping { editedLayout?.mapping ?? gameMapping }

    var editedLayout: ControllerLayout? { editing.flatMap(layouts.layout) }

    private var game: Game? { library.game(gameID) }

    private var gameMapping: ButtonMapping {
        ButtonMapping(stored: game?.buttonMapping, default: defaultLayout)
    }

    var allLayouts: [ControllerLayout] { layouts.all(for: game, in: library) }

    /// The layout the game shows, which the list marks.
    var currentLayout: ControllerLayout? { layouts.shown(for: gameMapping, game: game, in: library) }

    var baseline: ButtonMapping { editing == nil ? defaultLayout : ButtonMapping() }

    var defaultLayout: ButtonMapping {
        game.map { layouts.defaultLayout(for: $0, in: library) } ?? ButtonMapping()
    }

    var hasSuggestion: Bool { game.map { layouts.suggestion(for: $0) != nil } ?? false }

    func label(for input: ControlInput) -> String? {
        guard editing == nil, let game else { return nil }
        return layouts.label(for: input, game: game, in: library)
    }

    /// The button list's rows below the layouts: every button, with a row for each whole stick above its directions.
    static let inputRows: [Target] = ControlInput.allCases.flatMap { input -> [Target] in
        let stick = Stick.allCases.first { $0.directions.first == input }
        return (stick.map { [Target.stick($0)] } ?? []) + [.input(input)]
    }

    func columns(_ section: Section) -> Int {
        columns(minWidth: section.minWidth)
    }

    private func columns(minWidth: CGFloat) -> Int {
        max(1, Int((width + Self.spacing) / (minWidth + Self.spacing)))
    }

    func rows() -> [[Target]] {
        groups().flatMap(\.self)
    }

    /// The rows in groups that the shoulder buttons jump between: the layouts and the buttons, a saved layout's commands
    /// and its buttons, or each kind of action.
    private func groups() -> [[[Target]]] {
        if confirmingDelete { return [[[.confirmDelete, .cancelDelete]]] }
        if picking != nil { return Self.sections.map { Self.chunked($0.actions.map(Target.action), columns($0)) } }
        let buttons = Self.inputRows.map { [$0] }
        if editing != nil { return [[[.rename, .delete]], buttons] }
        let layoutRows = !listsLayouts ? [] : allLayouts.map { layout -> [Target] in
            if case .saved = layout.id { [.apply(layout.id), .edit(layout.id)] } else { [.apply(layout.id)] }
        }
        return [[[.layouts]] + layoutRows + [[.saveNew, .reset]], buttons]
    }

    private static func chunked(_ targets: [Target], _ columns: Int) -> [[Target]] {
        stride(from: 0, to: targets.count, by: columns).map { Array(targets[$0..<min($0 + columns, targets.count)]) }
    }

    /// The focused target, or the default when the stored one is not on screen.
    var focus: Target? {
        let rows = rows()
        let fallback: Target = if confirmingDelete {
            .cancelDelete
        } else if let picking {
            .action(mapping.action(for: picking))
        } else if editing == nil, let currentLayout {
            .apply(currentLayout.id)
        } else {
            .input(ControlInput.allCases[0])
        }
        for candidate in [target, fallback] {
            if let candidate, rows.contains(where: { $0.contains(candidate) }) { return candidate }
        }
        return rows.first?.first
    }

    func isFocused(_ candidate: Target) -> Bool { showsFocus && focus == candidate }

    /// Directions move focus, confirm activates it, and cancel steps back to the layout list. Returns false for buttons it
    /// does not use, so the caller can close the editor on cancel from the layout list.
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
            if confirmingDelete {
                activate(.cancelDelete)
            } else if picking != nil {
                closePicker()
            } else if editing != nil {
                stopEditing()
            } else if listsLayouts {
                activate(.layouts)
            } else {
                return false
            }
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

    func stopEditing() {
        target = editing.map(Target.edit)
        editing = nil
    }

    /// Asks for a name on the keyboard. A blank name saves nothing, and closing the keyboard keeps things as they were.
    private func openKeyboard(title: String, text: String, onDone: @escaping (String) -> Void) {
        var typed = text
        keyboard = OnScreenKeyboard(title: title, text: text) {
            typed = $0
        } onDone: {
            onDone(typed)
        } onClose: { [weak self] in
            self?.keyboard = nil
        }
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
        if let editing {
            layouts.update(editing, mapping: mapping, in: library)
        } else {
            let stored = mapping.stored(default: defaultLayout)
            library.update(gameID) { $0.buttonMapping = stored }
        }
    }

    private func activate(_ candidate: Target) {
        switch candidate {
        case let .input(input):
            picking = input
            target = nil
        case .layouts:
            listsLayouts.toggle()
            // Opening moves to the layout the game shows, or the first one when its buttons match none.
            let shown = currentLayout ?? allLayouts.first
            target = listsLayouts ? shown.map { Target.apply($0.id) } : .layouts
        case let .apply(id):
            if let layout = layouts.layout(id, for: game, in: library) { save(layout.mapping) }
            listsLayouts = false
            target = .layouts
        case .saveNew:
            let mapping = gameMapping
            guard currentLayout == nil else { return }
            openKeyboard(title: "Layout name", text: "") { [weak self] name in
                guard let self, layouts.add(name: name, mapping: mapping) != nil else { return }
                target = .layouts
            }
        case .reset:
            save(defaultLayout)
        case let .edit(id):
            editing = id
            target = nil
        case .rename:
            guard let editedLayout else { return }
            openKeyboard(title: "Layout name", text: editedLayout.name) { [weak self] name in
                self?.layouts.rename(editedLayout.id, to: name)
            }
        case .delete:
            confirmingDelete = true
            target = nil
        case .confirmDelete:
            if let editing { layouts.delete(editing) }
            confirmingDelete = false
            editing = nil
            target = nil
        case .cancelDelete:
            confirmingDelete = false
            target = .delete
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

    /// Which of the sheet's steps shows.
    private struct Step: Equatable {
        let editing: ControllerLayout.ID?
        let picking: ControlInput?
        let confirmingDelete: Bool

        init(_ editor: ButtonMappingEditor) {
            editing = editor.editing
            picking = editor.picking
            confirmingDelete = editor.confirmingDelete
        }
    }

    var body: some View {
        let game = hub.library.game(editor.gameID).map(hub.displayTitle) ?? ""
        let title = editor.picking.map { "Map \($0.title)" } ?? (editor.editing == nil ? "Controller layout" : "Edit layout")
        SheetScaffold(title: title, subtitle: editor.editedLayout?.name ?? game, onClose: onClose) {
            ScrollViewReader { proxy in
                ScrollView {
                    Group {
                        if editor.confirmingDelete {
                            deleteQuestion
                        } else if let input = editor.picking {
                            picker(for: input)
                        } else if editor.editing != nil {
                            layoutEditor
                        } else {
                            layoutList
                        }
                    }
                    .padding(.horizontal, 22)
                    .padding(.bottom, 22)
                }
                .onChange(of: editor.focus) { _, focus in
                    guard editor.showsFocus, let focus else { return }
                    withAnimation(Theme.focusAnimation) { proxy.scrollTo(focus, anchor: .center) }
                }
                // The scroll view keeps its offset in a sheet of one size, which would open Edit below its Rename and
                // Delete, so each step opens at its focus.
                .onChange(of: Step(editor)) {
                    if let focus = editor.focus { proxy.scrollTo(focus, anchor: .center) }
                }
                // The first press reveals the ring without moving focus, and focus can be scrolled away.
                .onChange(of: editor.showsFocus) { _, shows in
                    if shows, let focus = editor.focus { withAnimation(Theme.focusAnimation) { proxy.scrollTo(focus, anchor: .center) } }
                }
            }
            .onGeometryChange(for: CGFloat.self) { $0.size.width - 44 } action: { editor.width = $0 }
        }
        .onScreenKeyboard(editor.keyboard)
        // Typing on the keyboard reaches its text field rather than moving focus, as in the hub's other sheets.
        .onChange(of: editor.keyboard != nil) { _, typing in hub.input.translatesKeyboard = !typing }
        // A swipe can close the sheet under the keyboard.
        .onDisappear { if editor.keyboard != nil { hub.input.translatesKeyboard = true } }
        .presentationDetents([.large])
        #if os(macOS)
        // A Mac sheet takes its content's ideal size, which for a scroll view is everything inside it. Without one set
        // here, the sheet would resize between steps and move its buttons from under the pointer.
        .frame(minWidth: 520, idealWidth: 520, minHeight: 560, idealHeight: 560)
        #endif
    }

    /// The layout the game shows, which opens the list of layouts, then what the game's buttons do.
    private var layoutList: some View {
        let mapping = editor.mapping, defaultLayout = editor.defaultLayout
        let current = editor.currentLayout
        let defaultName = editor.allLayouts.first { $0.mapping == defaultLayout }?.name ?? "the default"
        return VStack(alignment: .leading, spacing: 8) {
            SectionLabel(text: "Layout")
            layoutChooser(current)
            if editor.listsLayouts {
                ForEach(editor.allLayouts) { layout in
                    layoutRow(layout, current: layout.id == current?.id)
                }
            }
            HStack(spacing: ButtonMappingEditor.spacing) {
                actionButton(.saveNew, title: "Save as layout", systemImage: "plus")
                    .disabled(current != nil)
                    .accessibilityLabel("Save as new layout")
                actionButton(.reset, title: "Reset", systemImage: "arrow.counterclockwise")
                    .disabled(mapping == defaultLayout)
                    .accessibilityLabel("Reset to \(defaultName)")
            }
            .padding(.vertical, 8)
            SectionLabel(text: "Buttons")
            buttonRows(mapping)
            note(footnote(defaultLayout, suggested: editor.hasSuggestion))
        }
        .animation(Theme.focusAnimation, value: editor.focus)
    }

    /// A saved layout's name and buttons, which change every game showing it.
    private var layoutEditor: some View {
        VStack(alignment: .leading, spacing: 8) {
            Button { editor.stopEditing() } label: { Label("All layouts", systemImage: "chevron.left") }
                .buttonStyle(HubButtonStyle(kind: .secondary, focused: false))
            note("Changes here also change every game that shows this layout, not only this game. A game you changed since picking the layout keeps its own buttons.")
                .padding(.bottom, 6)
            HStack(spacing: ButtonMappingEditor.spacing) {
                actionButton(.rename, title: "Rename", systemImage: "pencil")
                actionButton(.delete, title: "Delete", systemImage: "trash")
            }
            .padding(.bottom, 8)
            SectionLabel(text: "Buttons")
            buttonRows(editor.mapping)
        }
        .animation(Theme.focusAnimation, value: editor.focus)
    }

    private var deleteQuestion: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Delete \(editor.editedLayout?.name ?? "this layout")?").font(.system(size: 17, weight: .bold))
            note("Games using it keep their buttons.")
            HStack(spacing: ButtonMappingEditor.spacing) {
                actionButton(.confirmDelete, title: "Delete", systemImage: "trash")
                actionButton(.cancelDelete, title: "Cancel", systemImage: "xmark")
            }
        }
        .animation(Theme.focusAnimation, value: editor.focus)
    }

    /// The layout the game shows, or Custom, which opens and closes the list of layouts.
    private func layoutChooser(_ current: ControllerLayout?) -> some View {
        let target = ButtonMappingEditor.Target.layouts
        return Button { editor.select(target) } label: {
            HStack(spacing: 14) {
                Image(systemName: current?.id.systemImage ?? "slider.horizontal.3").frame(width: 26)
                Text(current?.name ?? "Custom").font(.system(size: 16, weight: .semibold)).lineLimit(1)
                Spacer()
                Text(editor.listsLayouts ? "Close" : "Change").foregroundStyle(Theme.secondaryText)
                Image(systemName: editor.listsLayouts ? "chevron.up" : "chevron.down").foregroundStyle(Theme.secondaryText)
            }
            .padding(12)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(rowFill(target), in: .rect(cornerRadius: 14, style: .continuous))
            .overlay { rowStroke(target) }
            .contentShape(.rect)
        }
        .buttonStyle(.plain)
        .accessibilityValue(current?.name ?? "Custom")
        .id(target)
    }

    /// A layout to pick, with an Edit button when it is one the player saved.
    private func layoutRow(_ layout: ControllerLayout, current: Bool) -> some View {
        let target = ButtonMappingEditor.Target.apply(layout.id)
        return HStack(spacing: ButtonMappingEditor.spacing) {
            Button { editor.select(target) } label: {
                HStack(spacing: 14) {
                    Image(systemName: layout.id.systemImage).frame(width: 26)
                    Text(layout.name).font(.system(size: 16, weight: .semibold)).lineLimit(1)
                    Spacer()
                    if current { Image(systemName: "checkmark.circle.fill").foregroundStyle(Theme.accent) }
                }
                .padding(12)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(rowFill(target), in: .rect(cornerRadius: 14, style: .continuous))
                .overlay { rowStroke(target) }
                .contentShape(.rect)
            }
            .buttonStyle(.plain)
            .accessibilityAddTraits(current ? .isSelected : [])
            .id(target)
            if case .saved = layout.id {
                let edit = ButtonMappingEditor.Target.edit(layout.id)
                Button("Edit") { editor.select(edit) }
                    .buttonStyle(HubButtonStyle(kind: .secondary, focused: editor.isFocused(edit)))
                    .id(edit)
            }
        }
    }

    private func actionButton(_ target: ButtonMappingEditor.Target, title: String, systemImage: String) -> some View {
        Button { editor.select(target) } label: {
            Label(title, systemImage: systemImage)
                .lineLimit(1)
                .minimumScaleFactor(0.7)
                .frame(maxWidth: .infinity)
        }
        .buttonStyle(HubButtonStyle(kind: .secondary, focused: editor.isFocused(target)))
        .id(target)
    }

    private func note(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 13))
            .foregroundStyle(Theme.tertiaryText)
            .fixedSize(horizontal: false, vertical: true)
            .padding(.top, 14)
    }

    private func buttonRows(_ mapping: ButtonMapping) -> some View {
        let baseline = editor.baseline
        return ForEach(ButtonMappingEditor.inputRows, id: \.self) { target in
            switch target {
            case let .input(input):
                row(target, name: input.title, action: mapping.action(for: input).title,
                    controllersOnly: !input.isOnTouchGamepad, suggested: editor.label(for: input),
                    changed: mapping.action(for: input) != baseline.action(for: input))
            case let .stick(stick):
                let isStick = stick.directions.allSatisfy { mapping.action(for: $0) == .gamepad($0) }
                row(target, name: stick.title,
                    action: mapping.movesPointer(stick) ? "Mouse movement" : isStick ? "Stick" : "Set by direction",
                    controllersOnly: true, detail: mapping.movesPointer(stick) ? "Use as a stick" : "Use as mouse movement")
            default:
                EmptyView()
            }
        }
    }

    private func footnote(_ defaultLayout: ButtonMapping, suggested: Bool) -> String {
        let suggestion = suggested
            ? "Suggested for this game puts keys this game's own files bind on buttons its engine leaves unused. Reset returns to it, and Xbox gamepad goes back to the engine's own buttons. "
            : ""
        let start = suggestion + Self.startingLayoutNote(defaultLayout)
        return start + " Picking a layout copies its buttons into this game, and changing a button changes only this game until you save it as a new layout. Home on a controller, the house button on the touch gamepad, and the backtick key always open the VNDegen menu. The library and the VNDegen menu keep their own navigation, so a change here never stops a controller from working them. LB and RB jump between groups."
    }

    static func startingLayoutNote(_ defaultLayout: ButtonMapping) -> String {
        switch ControllerLayout.builtIns.first(where: { $0.mapping == defaultLayout })?.id {
        case .visualNovel:
            "This game's engine reads no gamepad, so its games start with Visual novel: A presses Return, B right clicks, X holds Control, Y and Menu press Escape, LB turns the wheel up and RB down, the D-pad and left stick are the arrows, the right stick moves the pointer, RT and R3 click, and LT right clicks. Keyboard and mouse is the layout of Steam's template of that name."
        case .keyboardAndMouse:
            "This game's engine reads no gamepad, so its games start with Keyboard and mouse, the layout of Steam's template of that name: the left stick is WASD, the right stick moves the pointer, RT and R3 click, LT right clicks, LB and RB turn the wheel, and the other buttons press keys. Mouse puts the pointer on both sticks and the mouse buttons on A and X."
        case .mouse:
            "This game's engine reads no gamepad and its games are played with the mouse, so they start with Mouse: both sticks move the pointer, A and RT click, X and LT right click, LB and RB turn the wheel, B and Menu press Escape, and Y shows the keyboard. Keyboard and mouse puts keys on the other buttons."
        case .rpgMaker:
            "RPG Maker XP, VX, and VX Ace games start with RPG Maker controls, the keys RPG Maker reads: A is Return, B Escape, X Shift, Y and Menu X, LB Q, RB W, and LT, RT, and View the A, S, and D keys that games use for RPG Maker's X, Y, and Z buttons. The D-pad and left stick are the arrows."
        default:
            "Every button sends the same Xbox controller button unless you change it, and the game reads it as a gamepad."
        }
    }

    /// A button, or a whole stick, with what it does. A stick's row switches it between moving the pointer and being a
    /// stick, and says which it would become.
    private func row(_ target: ButtonMappingEditor.Target, name: String, action: String, controllersOnly: Bool,
                     suggested: String? = nil, changed: Bool = false, detail: String? = nil) -> some View {
        Button { editor.select(target) } label: {
            HStack(spacing: 14) {
                // One width for every button, so the actions line up.
                Text(name)
                    .font(.system(size: 14, weight: .heavy, design: .rounded))
                    .lineLimit(1)
                    .minimumScaleFactor(0.8)
                    .padding(.horizontal, 6)
                    .frame(width: 104, height: 32)
                    .background(Theme.surface, in: Capsule(style: .circular))
                    .overlay { Capsule(style: .circular).strokeBorder(Theme.stroke) }
                VStack(alignment: .leading, spacing: 2) {
                    Text(action).font(.system(size: 16, weight: .semibold))
                    if let suggested {
                        Text(suggested).font(.system(size: 13)).foregroundStyle(Theme.secondaryText)
                    }
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
        // Gamepad choices sit under their own Gamepad heading, so they leave the word off their names.
        let label = switch action {
        case let .key(key): key.label
        case let .gamepad(input): input.title
        default: action.title
        }
        return Button { editor.select(target) } label: {
            Text(label)
                .font(.system(size: 15, weight: .semibold))
                .lineLimit(1)
                .minimumScaleFactor(0.7)
                .foregroundStyle(focused ? .black : .white)
                .padding(.horizontal, 8)
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
