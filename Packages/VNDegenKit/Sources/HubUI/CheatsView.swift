import ControllerUI
import SwiftUI
import VNCore

/// The Cheats page of the in-game menu: what it shows, controller focus, and the changes it sends the game.
@MainActor
@Observable
final class CheatsPage {
    enum Tab: Hashable, CaseIterable {
        case quick, items, variables, switches

        var title: String {
            switch self {
            case .quick: "Quick"
            case .items: "Items"
            case .variables: "Variables"
            case .switches: "Switches"
            }
        }
    }

    enum Target: Hashable {
        case tab(Tab)
        case gold(Int)
        case restore, keepHealthy, noEncounters, walkThrough, speed, experience
        case itemKind(CheatItem.Kind)
        case search
        case item(CheatItem.ID, count: Int)
        case variable(Int)
        case toggle(Int)
    }

    static let goldAmounts = [1_000, 10_000, 100_000]

    let cheats: any CheatSession
    private(set) var settings: CheatSettings
    @ObservationIgnored private let saveSettings: (CheatSettings) -> Void
    private(set) var tab = Tab.quick
    /// Nil until the game first answers.
    private(set) var state: CheatState?
    private(set) var status: Status?
    var query = ""
    private(set) var itemKind = CheatItem.Kind.item
    var focus = FocusRing<Target>()
    /// The keyboard a controller types a search or a value on.
    private(set) var keyboard: OnScreenKeyboard?
    private var busy = false

    struct Status: Equatable {
        let text: String
        let isError: Bool
    }

    /// `saveSettings` keeps the cheats that stay on with the game, in the library.
    init(cheats: any CheatSession, settings: CheatSettings, saveSettings: @escaping (CheatSettings) -> Void) {
        self.cheats = cheats
        self.settings = settings
        self.saveSettings = saveSettings
    }

    var features: CheatFeatures { cheats.features }

    var tabs: [Tab] {
        Tab.allCases.filter { tab in
            switch tab {
            case .quick: !features.isDisjoint(with: [.gold, .restoreParty, .keepPartyHealthy, .noEncounters,
                                                      .walkThroughWalls, .speed, .experience])
            case .items: features.contains(.items)
            case .variables: features.contains(.variables)
            case .switches: features.contains(.switches)
            }
        }
    }

    var inGame: Bool { state?.inGame == true }

    // MARK: What each tab lists

    var shownItems: [CheatItem] {
        (state?.items ?? []).filter { $0.id.kind == itemKind && matches($0.name, id: $0.id.number) }
    }

    var shownVariables: [CheatVariable] { (state?.variables ?? []).filter { matches($0.name, id: $0.id) } }

    var shownSwitches: [CheatSwitch] { (state?.switches ?? []).filter { matches($0.name, id: $0.id) } }

    /// A name that contains the query, or the number it names, with or without a leading #.
    private func matches(_ name: String, id: Int) -> Bool {
        let query = query.trimmingCharacters(in: .whitespaces)
        if query.isEmpty { return true }
        let digits = query.hasPrefix("#") ? String(query.dropFirst()) : query
        return Int(digits) == id || name.localizedCaseInsensitiveContains(query)
    }

    /// Focus rows, top to bottom.
    var rows: [[Target]] {
        var rows: [[Target]] = [tabs.map(Target.tab)]
        switch tab {
        case .quick:
            if inGame, features.contains(.gold) { rows.append(Self.goldAmounts.map(Target.gold)) }
            if inGame, features.contains(.restoreParty) { rows.append([.restore]) }
            if features.contains(.keepPartyHealthy) { rows.append([.keepHealthy]) }
            if features.contains(.noEncounters) { rows.append([.noEncounters]) }
            if features.contains(.walkThroughWalls) { rows.append([.walkThrough]) }
            if features.contains(.speed) { rows.append([.speed]) }
            if features.contains(.experience) { rows.append([.experience]) }
        case .items:
            guard inGame else { break }
            rows.append(CheatItem.Kind.allCases.map(Target.itemKind))
            rows.append([.search])
            rows += shownItems.map { [.item($0.id, count: 1), .item($0.id, count: 10)] }
        case .variables:
            guard inGame else { break }
            rows.append([.search])
            rows += shownVariables.map { [.variable($0.id)] }
        case .switches:
            guard inGame else { break }
            rows.append([.search])
            rows += shownSwitches.map { [.toggle($0.id)] }
        }
        return rows
    }

    private var defaultTarget: Target { .tab(tab) }

    func isFocused(_ target: Target) -> Bool { focus.isFocused(target, in: rows, default: defaultTarget) }

    // MARK: Input

    /// Directions move focus, left and right also step the focused setting or value, LB and RB switch tabs, and confirm
    /// activates the focused control. Returns false for cancel, which leaves the page.
    func handle(_ button: VirtualButton) -> Bool {
        if let keyboard {
            keyboard.handle(button)
            return true
        }
        let current = focus.current(in: rows, default: defaultTarget)
        switch button {
        case .cancel: return false
        case .pageUp, .pageDown: switchTab(by: button == .pageUp ? -1 : 1)
        case .confirm:
            focus.showsFocus = true
            if let current { activate(current) }
        case .left, .right:
            if focus.showsFocus, let current, steps(current) {
                step(current, by: button == .left ? -1 : 1)
            } else {
                moveFocus(button)
            }
        default: moveFocus(button)
        }
        return true
    }

    private func moveFocus(_ button: VirtualButton) {
        _ = focus.move(button, in: rows, default: defaultTarget)
        // The tab under focus is the one showing, as with tabs anywhere.
        if case let .tab(tab)? = focus.target, tab != self.tab { show(tab) }
    }

    /// The pointer over a control focuses it, unless a controller is showing the ring. A Mac's pointer resting over the
    /// page would otherwise take focus each time rows move under it, such as when the game's state arrives.
    func hover(_ target: Target) {
        guard !focus.showsFocus else { return }
        focus.target = target
    }

    /// Touch and pointer: focus the control without showing the ring, then activate it.
    func select(_ target: Target) {
        focus.select(target)
        activate(target)
    }

    /// Controls that left and right change rather than leave.
    private func steps(_ target: Target) -> Bool {
        switch target {
        case .speed, .experience: true
        case let .variable(id): if case .integer = variable(id)?.value { true } else { false }
        default: false
        }
    }

    private func step(_ target: Target, by step: Int) {
        switch target {
        case .speed: change { $0.speed = Self.stepped(CheatSettings.speeds, from: $0.speed, by: step) }
        case .experience: change { $0.experienceRate = Self.stepped(CheatSettings.experienceRates, from: $0.experienceRate, by: step) }
        case let .variable(id):
            guard case let .integer(value)? = variable(id)?.value else { return }
            let range = cheats.variableRange
            let stepped = value.addingReportingOverflow(step)
            if !stepped.overflow, range.contains(stepped.partialValue) { setVariable(id, to: .integer(stepped.partialValue)) }
        default: break
        }
    }

    /// The choice `step` places along from `current`, stopping at either end.
    static func stepped(_ choices: [Int], from current: Int, by step: Int) -> Int {
        let index = choices.firstIndex(of: current) ?? 0
        return choices[min(choices.count - 1, max(0, index + step))]
    }

    private func activate(_ target: Target) {
        switch target {
        case let .tab(tab): show(tab)
        case let .gold(amount):
            run(.addGold(amount), done: "Added \(amount.formatted()) \(state?.currencyUnit ?? "")")
        case .restore: run(.restoreParty, done: "The party is back to full HP and MP")
        case .keepHealthy: change { $0.keepsPartyHealthy.toggle() }
        case .noEncounters: change { $0.noEncounters.toggle() }
        case .walkThrough: change { $0.walksThroughWalls.toggle() }
        case .speed: change { $0.speed = Self.cycled(CheatSettings.speeds, from: $0.speed) }
        case .experience: change { $0.experienceRate = Self.cycled(CheatSettings.experienceRates, from: $0.experienceRate) }
        case let .itemKind(kind):
            itemKind = kind
            focus.target = target
        case .search: openSearch()
        case let .item(id, count):
            let name = state?.items.first { $0.id == id }?.name ?? "item"
            run(.addItem(id, count: count), done: "Added \(count) \(name)")
        case let .variable(id): if let variable = variable(id) { edit(variable) }
        case let .toggle(id):
            guard let toggle = state?.switches.first(where: { $0.id == id }) else { return }
            setSwitch(id, to: !toggle.isOn)
        }
    }

    /// Confirm on a setting with choices moves to the next one, back to the first after the last.
    static func cycled(_ choices: [Int], from current: Int) -> Int {
        choices[((choices.firstIndex(of: current) ?? 0) + 1) % choices.count]
    }

    func show(_ tab: Tab) {
        guard tab != self.tab else { return }
        self.tab = tab
        query = ""
        if focus.target != .tab(tab) { focus.target = .tab(tab) }
    }

    private func switchTab(by step: Int) {
        let tabs = tabs
        guard let index = tabs.firstIndex(of: tab) else { return }
        show(tabs[(index + step + tabs.count) % tabs.count])
    }

    func openSearch() {
        keyboard = OnScreenKeyboard(title: "Search", text: query) { [weak self] in
            self?.query = $0
        } onClose: { [weak self] in
            self?.keyboard = nil
        }
    }

    // MARK: Changes

    /// A setting that stays on: saved with the game, then sent to it.
    func change(_ edit: (inout CheatSettings) -> Void) {
        edit(&settings)
        saveSettings(settings)
        cheats.apply(settings)
    }

    func variable(_ id: Int) -> CheatVariable? { state?.variables.first { $0.id == id } }

    /// Whole numbers and text open the keyboard on the value. The change goes to the game when the player finishes.
    func edit(_ variable: CheatVariable) {
        let text: String
        switch variable.value {
        case let .integer(value): text = String(value)
        case let .text(value): text = value
        case .other: return
        }
        var typed = text
        let title = variable.name.isEmpty ? "Variable \(variable.id)" : variable.name
        keyboard = OnScreenKeyboard(title: title, text: text) {
            typed = $0
        } onDone: { [weak self] in
            self?.finishEditing(variable, typed: typed)
        } onClose: { [weak self] in
            self?.keyboard = nil
        }
    }

    private func finishEditing(_ variable: CheatVariable, typed: String) {
        switch variable.value {
        case .integer:
            let range = cheats.variableRange
            guard let number = Int(typed.trimmingCharacters(in: .whitespaces)), range.contains(number) else {
                status = Status(text: "Variable \(variable.id) takes a whole number from \(range.lowerBound.formatted()) to \(range.upperBound.formatted())",
                                isError: true)
                return
            }
            setVariable(variable.id, to: .integer(number))
        case .text: setVariable(variable.id, to: .text(typed))
        case .other: break
        }
    }

    func setVariable(_ id: Int, to value: CheatValue) {
        run(.setVariable(id: id, value)) { state in
            guard let index = state.variables.firstIndex(where: { $0.id == id }) else { return }
            let old = state.variables[index]
            state.variables[index] = CheatVariable(id: id, name: old.name, value: value)
        }
    }

    func setSwitch(_ id: Int, to on: Bool) {
        run(.setSwitch(id: id, on)) { state in
            guard let index = state.switches.firstIndex(where: { $0.id == id }) else { return }
            let old = state.switches[index]
            state.switches[index] = CheatSwitch(id: id, name: old.name, isOn: on)
        }
    }

    /// Sends `action`, then shows `done`. A variable or switch changes in place; anything else reads the game again.
    private func run(_ action: CheatAction, done: String? = nil, update: ((inout CheatState) -> Void)? = nil) {
        guard !busy, inGame else { return }
        busy = true
        Task {
            defer { busy = false }
            do {
                try await cheats.perform(action)
                if let update, var state {
                    update(&state)
                    self.state = state
                } else {
                    await load()
                }
                if let done { status = Status(text: done, isError: false) }
            } catch {
                status = Status(text: error.localizedDescription, isError: true)
            }
        }
    }

    func load() async {
        do {
            state = try await cheats.state()
        } catch {
            status = Status(text: "Couldn't read the game: \(error.localizedDescription)", isError: true)
        }
    }
}

/// The Cheats page's card.
struct CheatsView: View {
    @Environment(\.hubMetrics) private var metrics
    let page: CheatsPage
    let showsFocus: Bool
    let onBack: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            // A landscape phone has room for about two rows of a list below the header and the tabs, one above the other.
            if metrics.isShort {
                HStack(spacing: 16) {
                    header
                    tabBar
                }
            } else {
                header
                tabBar
            }
            ScrollViewReader { proxy in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 6) { content }
                        .padding(4)
                }
                .scrollIndicators(.hidden)
                .onChange(of: page.focus.target) { _, target in
                    guard page.focus.showsFocus, let target else { return }
                    withAnimation(.easeOut(duration: 0.15)) { proxy.scrollTo(target) }
                }
            }
            if let status = page.status {
                Label(status.text, systemImage: status.isError ? "exclamationmark.triangle.fill" : "checkmark.circle.fill")
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(status.isError ? Theme.accent : Theme.secondaryText)
                    .lineLimit(2)
            }
        }
        .frame(width: min(560, max(0, metrics.size.width - 64)))
        .frame(maxHeight: max(200, metrics.size.height - (metrics.isShort ? 60 : 90)))
        .task { await page.load() }
    }

    private var header: some View {
        Button(action: onBack) {
            HStack(spacing: 8) {
                Image(systemName: "chevron.left").font(.system(size: 15, weight: .bold))
                Text("Cheats").font(.display(17, weight: .heavy))
            }
            .foregroundStyle(.white)
            .frame(minHeight: 34)
            .contentShape(.rect)
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Back to the VNDegen menu")
    }

    private var tabBar: some View {
        HStack(spacing: 4) {
            ForEach(page.tabs, id: \.self) { tab in
                segment(tab.title, selected: page.tab == tab, target: .tab(tab))
            }
        }
        .padding(4)
        .background(Theme.surface, in: .capsule)
    }

    @ViewBuilder private var content: some View {
        if page.state == nil {
            ProgressView().frame(maxWidth: .infinity).padding(20)
        } else {
            switch page.tab {
            case .quick: quick
            case .items: items
            case .variables: variables
            case .switches: switches
            }
        }
    }

    // MARK: Quick

    @ViewBuilder private var quick: some View {
        if !page.inGame {
            note("Start or load a game for gold and the party. The settings below work now, and stay on for this game.")
        }
        if page.inGame, page.features.contains(.gold), let state = page.state {
            caption("Gold: \((state.gold ?? 0).formatted()) \(state.currencyUnit)")
            HStack(spacing: 6) {
                ForEach(CheatsPage.goldAmounts, id: \.self) { amount in
                    button("+\(amount.formatted())", target: .gold(amount))
                }
            }
        }
        if page.inGame, page.features.contains(.restoreParty) {
            button("Restore HP and MP", systemImage: "cross.case.fill", target: .restore)
            if let party = page.state?.party, !party.isEmpty {
                VStack(alignment: .leading, spacing: 2) {
                    ForEach(Array(party.enumerated()), id: \.offset) { _, member in
                        Text("\(member.name)  HP \(member.hp)/\(member.maxHP)  MP \(member.mp)/\(member.maxMP)")
                    }
                }
                .font(.system(size: 12, weight: .medium).monospacedDigit())
                .foregroundStyle(Theme.secondaryText)
                .padding(.horizontal, 6)
            }
        }
        if page.features.contains(.keepPartyHealthy) {
            toggle("Keep HP and MP full", on: page.settings.keepsPartyHealthy, target: .keepHealthy)
        }
        if page.features.contains(.noEncounters) {
            toggle("No random encounters", on: page.settings.noEncounters, target: .noEncounters)
        }
        if page.features.contains(.walkThroughWalls) {
            toggle("Walk through walls", on: page.settings.walksThroughWalls, target: .walkThrough)
        }
        if page.features.contains(.speed) {
            choices("Game speed", CheatSettings.speeds, selected: page.settings.speed, target: .speed) { speed in
                page.change { $0.speed = speed }
            }
        }
        if page.features.contains(.experience) {
            choices("Experience", CheatSettings.experienceRates, selected: page.settings.experienceRate, target: .experience) { rate in
                page.change { $0.experienceRate = rate }
            }
        }
    }

    // MARK: Items

    @ViewBuilder private var items: some View {
        if !page.inGame {
            note("Start or load a game first.")
        } else {
            HStack(spacing: 4) {
                ForEach(CheatItem.Kind.allCases, id: \.self) { kind in
                    segment(kind.title, selected: page.itemKind == kind, target: .itemKind(kind))
                }
            }
            .padding(4)
            .background(Theme.surface, in: .capsule)
            searchField
            ForEach(page.shownItems) { item in
                HStack(spacing: 6) {
                    row(target: .item(item.id, count: 1), action: { page.select(.item(item.id, count: 1)) }) {
                        Text(item.name).lineLimit(1)
                        Spacer(minLength: 8)
                        Text("\(item.owned)").monospacedDigit().opacity(0.7)
                        Image(systemName: "plus")
                    }
                    button("+10", target: .item(item.id, count: 10))
                        .frame(width: 76)
                }
            }
        }
    }

    // MARK: Variables and switches

    @ViewBuilder private var variables: some View {
        if !page.inGame {
            note("Start or load a game first.")
        } else {
            note(Self.undoNote)
            searchField
            ForEach(page.shownVariables) { variable in
                row(target: .variable(variable.id), action: { page.select(.variable(variable.id)) }) {
                    number(variable.id)
                    Text(variable.name).lineLimit(1)
                    Spacer(minLength: 8)
                    value(variable.value)
                }
                .disabled({ if case .other = variable.value { true } else { false } }())
            }
        }
    }

    @ViewBuilder private var switches: some View {
        if !page.inGame {
            note("Start or load a game first.")
        } else {
            note(Self.undoNote)
            searchField
            ForEach(page.shownSwitches) { toggle in
                row(target: .toggle(toggle.id), action: { page.select(.toggle(toggle.id)) }) {
                    number(toggle.id)
                    Text(toggle.name).lineLimit(1)
                    Spacer(minLength: 8)
                    Text(toggle.isOn ? "ON" : "OFF")
                        .font(.system(size: 12, weight: .heavy))
                        .foregroundStyle(toggle.isOn ? Theme.accent : Theme.tertiaryText)
                }
            }
        }
    }

    static let undoNote = "Changes reach the game at once. To undo them, load a save you made before changing anything."

    private func number(_ id: Int) -> some View {
        Text(String(format: "%04d", id))
            .font(.system(size: 12, weight: .semibold).monospacedDigit())
            .opacity(0.6)
    }

    @ViewBuilder private func value(_ value: CheatValue) -> some View {
        switch value {
        case let .integer(number): Text(String(number)).monospacedDigit()
        case let .text(text): Text("\"\(text)\"").lineLimit(1)
        case let .other(description): Text(description).lineLimit(1).opacity(0.5)
        }
    }

    // MARK: Pieces

    private var searchField: some View {
        let focused = showsFocus && page.isFocused(.search)
        return Button { page.select(.search) } label: {
            HStack(spacing: 10) {
                Image(systemName: "magnifyingglass")
                Text(page.query.isEmpty ? "Search by name or number" : page.query)
                    .foregroundStyle(page.query.isEmpty ? (focused ? .black.opacity(0.6) : Theme.tertiaryText) : (focused ? .black : .white))
                    .lineLimit(1)
                Spacer()
            }
            .font(.system(size: 15, weight: .medium))
            .foregroundStyle(focused ? .black : .white)
            .padding(.horizontal, 14)
            .frame(minHeight: 40)
            .background(focused ? AnyShapeStyle(.white) : AnyShapeStyle(Theme.surface), in: .rect(cornerRadius: 12, style: .continuous))
            .overlay { RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(focused ? .white : Theme.stroke) }
            .contentShape(.rect)
        }
        .buttonStyle(.plain)
        .id(CheatsPage.Target.search)
        .onHover { if $0 { page.hover(.search) } }
    }

    private func note(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 13))
            .foregroundStyle(Theme.secondaryText)
            .fixedSize(horizontal: false, vertical: true)
            .padding(.horizontal, 6)
            .padding(.bottom, 2)
    }

    private func caption(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 13, weight: .semibold).monospacedDigit())
            .foregroundStyle(Theme.secondaryText)
            .padding(.horizontal, 6)
            .padding(.top, 4)
    }

    private func button(_ title: String, systemImage: String? = nil, target: CheatsPage.Target) -> some View {
        row(target: target, action: { page.select(target) }) {
            Spacer(minLength: 0)
            if let systemImage { Image(systemName: systemImage) }
            Text(title).lineLimit(1)
            Spacer(minLength: 0)
        }
    }

    private func toggle(_ title: String, on: Bool, target: CheatsPage.Target) -> some View {
        row(target: target, action: { page.select(target) }) {
            Text(title)
            Spacer(minLength: 8)
            Image(systemName: on ? "checkmark.circle.fill" : "circle")
                .foregroundStyle(on ? Theme.accent : Theme.tertiaryText)
        }
        .accessibilityAddTraits(on ? .isSelected : [])
    }

    /// One focus target for the row. Left and right step through the choices.
    private func choices(_ title: String, _ values: [Int], selected: Int, target: CheatsPage.Target,
                         choose: @escaping (Int) -> Void) -> some View {
        let focused = showsFocus && page.isFocused(target)
        return HStack(spacing: 10) {
            Text(title)
                .font(.system(size: 15, weight: .semibold))
                .foregroundStyle(focused ? .black : .white)
            Spacer(minLength: 8)
            HStack(spacing: 4) {
                ForEach(values, id: \.self) { value in
                    Button {
                        page.focus.select(target)
                        choose(value)
                    } label: {
                        SegmentLabel(title: "\(value)x", selected: value == selected)
                    }
                    .buttonStyle(.plain)
                    .accessibilityAddTraits(value == selected ? .isSelected : [])
                }
            }
            .padding(3)
            .background(Color.black.opacity(0.35), in: .capsule)
        }
        .padding(.leading, 14)
        .padding(.trailing, 4)
        .frame(minHeight: 44)
        .background(focused ? AnyShapeStyle(.white) : AnyShapeStyle(Theme.surface), in: .rect(cornerRadius: 12, style: .continuous))
        .overlay { RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(focused ? .white : Theme.stroke) }
        .id(target)
        .onHover { if $0 { page.hover(target) } }
    }

    private func row(target: CheatsPage.Target, action: @escaping () -> Void,
                     @ViewBuilder label: () -> some View) -> some View {
        let focused = showsFocus && page.isFocused(target)
        return Button(action: action) {
            HStack(spacing: 10) { label() }
                .font(.system(size: 15, weight: .semibold))
                .foregroundStyle(focused ? .black : .white)
                .padding(.horizontal, 14)
                .frame(maxWidth: .infinity, minHeight: 44, alignment: .leading)
                .background(focused ? AnyShapeStyle(.white) : AnyShapeStyle(Theme.surface), in: .rect(cornerRadius: 12, style: .continuous))
                .overlay { RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(focused ? .white : Theme.stroke) }
                .contentShape(.rect)
        }
        .buttonStyle(.plain)
        .id(target)
        .onHover { if $0 { page.hover(target) } }
    }

    private func segment(_ title: String, selected: Bool, target: CheatsPage.Target) -> some View {
        let focused = showsFocus && page.isFocused(target)
        return Button { page.select(target) } label: {
            SegmentLabel(title: title, selected: selected)
                .overlay { Capsule().strokeBorder(.white, lineWidth: focused ? 2.5 : 0) }
        }
        .buttonStyle(.plain)
        .id(target)
        .accessibilityAddTraits(selected ? .isSelected : [])
        .onHover { if $0 { page.hover(target) } }
    }
}
