import SwiftUI
import PhotosUI
import UniformTypeIdentifiers
import VNCore
import Metadata
import ControllerUI

/// Shared chrome for the hub's sheets: a title, a close button, and the dark surface.
struct SheetScaffold<Content: View>: View {
    @Environment(Navigator.self) private var nav
    let title: String
    var subtitle: String?
    /// Only for sheets that turn off keyboard translation. Elsewhere InputHub already maps Escape to cancel.
    var escapeCloses = false
    /// Defaults to closing the navigator's sheet.
    var onClose: (() -> Void)?
    @ViewBuilder var content: Content

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack(alignment: .firstTextBaseline) {
                VStack(alignment: .leading, spacing: 4) {
                    Text(title).font(.display(22))
                    if let subtitle {
                        Text(subtitle).font(.system(size: 14)).foregroundStyle(Theme.secondaryText).lineLimit(1)
                    }
                }
                Spacer()
                Button("Done") { if let onClose { onClose() } else { nav.back() } }
                    .buttonStyle(HubButtonStyle(kind: .secondary, focused: false))
                    .keyboardShortcut(escapeCloses ? .cancelAction : nil)
            }
            .padding(22)
            content
        }
        .foregroundStyle(.white)
        .background(Theme.background.opacity(0.98))
        .preferredColorScheme(.dark)
    }
}

// MARK: Choice sheets

/// Picks one value for a game, such as its engine override or in-game orientation.
struct ChoiceSheet: View {
    @Environment(Navigator.self) private var nav
    let sheet: Navigator.Sheet
    let title: String
    let subtitle: String
    let footnote: String

    var body: some View {
        let selected = nav.selectedChoice(for: sheet)
        SheetScaffold(title: title, subtitle: subtitle) {
            ScrollViewReader { proxy in
            ScrollView {
                VStack(spacing: 10) {
                    ForEach(nav.choices(for: sheet)) { choice in
                        let focused = nav.isFocused(choice.target)
                        let isSelected = selected == choice.target
                        Button { nav.select(choice.target) } label: {
                            HStack(spacing: 14) {
                                Image(systemName: choice.systemImage ?? (isSelected ? "checkmark.circle.fill" : "circle"))
                                    .font(.system(size: 20))
                                    .foregroundStyle(isSelected || choice.systemImage != nil ? Theme.accent : Theme.tertiaryText)
                                    .frame(width: 26)
                                VStack(alignment: .leading, spacing: 2) {
                                    Text(choice.title).font(.system(size: 16, weight: .semibold))
                                    if let detail = choice.detail {
                                        Text(detail).font(.system(size: 13)).foregroundStyle(Theme.secondaryText)
                                    }
                                }
                                Spacer()
                            }
                            .padding(16)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .background(focused ? Color.white.opacity(0.16) : Theme.surface, in: .rect(cornerRadius: 14, style: .continuous))
                            .overlay {
                                RoundedRectangle(cornerRadius: 14, style: .continuous)
                                    .strokeBorder(focused ? .white : Theme.stroke, lineWidth: focused ? 2 : 1)
                            }
                            .contentShape(.rect)
                        }
                        .buttonStyle(.plain)
                        .onHover { if $0 { nav.hover(choice.target) } }
                        .animation(Theme.focusAnimation, value: focused)
                        .id(choice.target)
                    }
                    Text(footnote)
                        .font(.system(size: 13))
                        .foregroundStyle(Theme.tertiaryText)
                        .padding(.top, 6)
                }
                .padding(.horizontal, 22)
                .padding(.bottom, 22)
            }
            // A long list runs below a medium sheet, so the controller's focus scrolls into view.
            .onChange(of: nav.focus) { _, focus in
                guard nav.focusMovedByController, let focus else { return }
                withAnimation(Theme.focusAnimation) { proxy.scrollTo(focus, anchor: .center) }
            }
            }
        }
        .presentationDetents([.medium, .large])
    }
}

// MARK: Artwork editor

extension ArtworkSlot {
    var title: String {
        switch self {
        case .capsule: "Capsule"
        case .hero: "Hero"
        case .logo: "Logo"
        case .header: "Header"
        case .icon: "Icon"
        }
    }

    var hint: String {
        switch self {
        case .capsule: "Portrait library tile, 600 × 900"
        case .hero: "Wide banner behind the game page, 3840 × 1240"
        case .logo: "Transparent title logo drawn over the hero"
        case .header: "Landscape tile, 920 × 430"
        case .icon: "Square icon, 256 × 256"
        }
    }
}

/// The artwork sheet's state. The sheet holds it so controller focus can reach the suggestions.
@MainActor
@Observable
final class ArtworkEditor {
    let gameID: GameID
    var slot: ArtworkSlot = .capsule
    private(set) var candidates: [ArtworkCandidate] = []
    private(set) var loading = false
    private(set) var applying: ArtworkCandidate.ID?
    var error: String?
    /// Set by the sheet from its width, so the focus table matches the suggestion grid.
    var columns = 3
    private let hub: Hub

    init(hub: Hub, gameID: GameID) {
        self.hub = hub
        self.gameID = gameID
    }

    func cycleSlot(_ step: Int) {
        let slots = ArtworkSlot.allCases
        let index = slots.firstIndex(of: slot) ?? 0
        slot = slots[(index + step + slots.count) % slots.count]
    }

    func loadCandidates() async {
        guard let game = hub.library.game(gameID) else { return }
        loading = true
        candidates = await hub.metadata.artworkCandidates(for: game, slot: slot)
        loading = false
    }

    func apply(_ candidate: ArtworkCandidate) {
        guard applying == nil else { return }
        applying = candidate.id
        error = nil
        Task {
            do {
                try await hub.metadata.setArtwork(candidate, for: gameID, in: hub.library)
            } catch {
                self.error = "Couldn't download that image: \(error.localizedDescription)"
            }
            applying = nil
        }
    }
}

/// Steam's "set custom artwork": pick a slot, then a suggested image, a photo, or a file.
struct ArtworkEditorSheet: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav
    @Environment(\.hubMetrics) private var metrics
    let game: Game
    let editor: ArtworkEditor

    @State private var photo: PhotosPickerItem?
    @State private var importingFile = false

    var body: some View {
        let slot = editor.slot
        SheetScaffold(title: "Artwork", subtitle: hub.displayTitle(game)) {
            ScrollViewReader { proxy in
                ScrollView {
                    VStack(alignment: .leading, spacing: 22) {
                        slotTabs(slot)

                        HStack(alignment: .bottom, spacing: 18) {
                            preview(slot)
                            VStack(alignment: .leading, spacing: 10) {
                                Text(slot.hint).font(.system(size: 13)).foregroundStyle(Theme.secondaryText)
                                PhotosPicker(selection: $photo, matching: .images) {
                                    Label("Photos", systemImage: "photo")
                                }
                                .buttonStyle(HubButtonStyle(kind: .secondary, focused: false))
                                Button { importingFile = true } label: { Label("Files", systemImage: "folder") }
                                    .buttonStyle(HubButtonStyle(kind: .secondary, focused: false))
                                if hub.input.lastInputKind == .gamepad {
                                    Text("Photos and Files need touch or a mouse. With a controller, pick a suggestion below.")
                                        .font(.system(size: 13))
                                        .foregroundStyle(Theme.tertiaryText)
                                        .fixedSize(horizontal: false, vertical: true)
                                }
                            }
                        }

                        if let error = editor.error {
                            Label(error, systemImage: "exclamationmark.triangle.fill")
                                .font(.system(size: 13))
                                .foregroundStyle(Theme.accent)
                        }

                        SectionLabel(text: "Suggestions")
                        suggestions(slot)
                    }
                    .padding(.horizontal, 22)
                    .padding(.bottom, 28)
                }
                .onChange(of: nav.focus) { _, focus in
                    guard nav.focusMovedByController, case .artworkCandidate? = focus else { return }
                    withAnimation(Theme.focusAnimation) { proxy.scrollTo(focus, anchor: .center) }
                }
            }
        }
        .task(id: slot) { await editor.loadCandidates() }
        .onChange(of: photo) { _, item in
            guard let item else { return }
            Task { await importPhoto(item, slot: slot) }
        }
        .fileImporter(isPresented: $importingFile, allowedContentTypes: [.image]) { result in
            importFile(result, slot: slot)
        }
        .presentationDetents([.large])
        .presentationSizing(.page)
    }

    /// One tab per slot. A row of focus targets rather than a segmented picker, so a controller can show focus on a slot.
    private func slotTabs(_ selected: ArtworkSlot) -> some View {
        HStack(spacing: 6) {
            ForEach(ArtworkSlot.allCases, id: \.self) { slot in
                let target = Navigator.Target.artworkSlot(slot)
                let focused = nav.isFocused(target)
                Button { nav.select(target) } label: {
                    Text(slot.title)
                        .font(.system(size: 13, weight: .semibold))
                        .lineLimit(1)
                        .minimumScaleFactor(0.8)
                        .foregroundStyle(focused ? .black : .white)
                        .frame(maxWidth: .infinity, minHeight: 34)
                        .background {
                            Capsule().fill(focused ? AnyShapeStyle(.white) : slot == selected ? AnyShapeStyle(Theme.accent) : AnyShapeStyle(.clear))
                        }
                        .contentShape(.capsule)
                }
                .buttonStyle(.plain)
                .onHover { if $0 { nav.hover(target) } }
                .accessibilityAddTraits(slot == selected ? .isSelected : [])
                .id(target)
            }
        }
        .padding(4)
        .background(Theme.surface, in: .capsule)
        .overlay { Capsule().strokeBorder(Theme.stroke) }
        .animation(Theme.focusAnimation, value: nav.focus)
    }

    private func preview(_ slot: ArtworkSlot) -> some View {
        let aspect = slot.nominalSize.width / slot.nominalSize.height
        let height: CGFloat = metrics.isCompact ? 150 : 210
        let width = min(height * aspect, metrics.isCompact ? metrics.size.width * 0.5 : 460)
        return Group {
            if slot == .logo {
                GameLogo(game: game, maxSize: CGSize(width: width * 0.9, height: width / aspect * 0.8), alignment: .center)
                    .frame(width: width, height: width / aspect)
                    .background { GameArt(game: game, slots: [.hero, .header], size: CGSize(width: width, height: width / aspect)).opacity(0.5) }
            } else {
                GameArt(game: game, slots: [slot], size: CGSize(width: width, height: width / aspect))
            }
        }
        .clipShape(.rect(cornerRadius: 10, style: .continuous))
        .overlay { RoundedRectangle(cornerRadius: 10, style: .continuous).strokeBorder(Theme.stroke) }
        .overlay(alignment: .topTrailing) {
            if game.artwork[slot] == nil {
                Tag(text: "Generated").padding(8)
            }
        }
    }

    @ViewBuilder
    private func suggestions(_ slot: ArtworkSlot) -> some View {
        if editor.loading {
            ProgressView().frame(maxWidth: .infinity, minHeight: 120)
        } else if editor.candidates.isEmpty {
            Text("No suggestions for this slot. Add a SteamGridDB key in Settings, or match the game with Find metadata.")
                .font(.system(size: 14))
                .foregroundStyle(Theme.tertiaryText)
                .frame(maxWidth: .infinity, minHeight: 80, alignment: .leading)
        } else {
            let aspect = slot.nominalSize.width / slot.nominalSize.height
            let minWidth: CGFloat = aspect < 1 ? 110 : 200
            let spacing: CGFloat = 12
            // A fixed column count instead of adaptive columns, so the navigator's focus rows match what is drawn.
            LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: spacing), count: max(1, editor.columns)), spacing: spacing) {
                ForEach(Array(editor.candidates.enumerated()), id: \.element.id) { index, candidate in
                    let target = Navigator.Target.artworkCandidate(index)
                    Button { nav.select(target) } label: {
                        AsyncImage(url: candidate.thumbnail ?? candidate.url) { image in
                            image.resizable().aspectRatio(contentMode: slot == .logo ? .fit : .fill)
                        } placeholder: {
                            Theme.surface
                        }
                        .aspectRatio(aspect, contentMode: .fit)
                        .frame(maxWidth: .infinity)
                        .clipShape(.rect(cornerRadius: 8, style: .continuous))
                        .overlay {
                            if editor.applying == candidate.id {
                                ZStack { Color.black.opacity(0.5); ProgressView() }
                                    .clipShape(.rect(cornerRadius: 8, style: .continuous))
                            }
                        }
                        .overlay(alignment: .bottomLeading) { Tag(text: candidate.source.displayName).padding(6) }
                    }
                    .buttonStyle(TileButtonStyle(focused: nav.isFocused(target)))
                    .onHover { if $0 { nav.hover(target) } }
                    .disabled(editor.applying != nil)
                    .id(target)
                }
            }
            .onGeometryChange(for: Int.self) { geo in
                max(1, Int((geo.size.width + spacing) / (minWidth + spacing)))
            } action: { editor.columns = $0 }
        }
    }

    private func importPhoto(_ item: PhotosPickerItem, slot: ArtworkSlot) async {
        defer { photo = nil }
        do {
            guard let data = try await item.loadTransferable(type: Data.self) else { return }
            let ext = item.supportedContentTypes.first?.preferredFilenameExtension ?? "jpg"
            try hub.library.setArtwork(data, fileExtension: ext, slot: slot, for: game.id)
            editor.error = nil
        } catch {
            editor.error = "Couldn't import that photo: \(error.localizedDescription)"
        }
    }

    private func importFile(_ result: Result<URL, any Error>, slot: ArtworkSlot) {
        do {
            let url = try result.get()
            let scoped = url.startAccessingSecurityScopedResource()
            defer { if scoped { url.stopAccessingSecurityScopedResource() } }
            let data = try Data(contentsOf: url)
            try hub.library.setArtwork(data, fileExtension: url.pathExtension.isEmpty ? "png" : url.pathExtension, slot: slot, for: game.id)
            editor.error = nil
        } catch {
            editor.error = "Couldn't import that file: \(error.localizedDescription)"
        }
    }
}

// MARK: Metadata search

/// The metadata search sheet's state. The sheet holds it so controller focus can reach the results.
@MainActor
@Observable
final class MetadataSearch {
    let gameID: GameID
    var query = ""
    private(set) var results: [MetadataCandidate]?
    private(set) var searching = false
    private(set) var applying: MetadataCandidate.ID?
    var error: String?
    /// True while the search field has keyboard focus, after a tap or click.
    var editingQuery = false
    /// The keyboard a controller types the title on. Finishing it searches.
    private(set) var keyboard: OnScreenKeyboard?
    private let hub: Hub

    init(hub: Hub, gameID: GameID) {
        self.hub = hub
        self.gameID = gameID
    }

    func search() async {
        let title = query.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !title.isEmpty else { return }
        searching = true
        results = await hub.metadata.search(title)
        searching = false
    }

    func openKeyboard() {
        keyboard = OnScreenKeyboard(title: "Title", text: query) { [weak self] in
            self?.query = $0
        } onDone: { [weak self] in
            Task { await self?.search() }
        } onClose: { [weak self] in
            self?.keyboard = nil
        }
    }

    /// Returns whether the match was applied.
    func apply(_ candidate: MetadataCandidate) async -> Bool {
        guard applying == nil else { return false }
        applying = candidate.id
        error = nil
        defer { applying = nil }
        do {
            try await hub.metadata.apply(candidate, to: gameID, in: hub.library)
            return true
        } catch {
            self.error = "Couldn't apply \(candidate.title): \(error.localizedDescription)"
            return false
        }
    }
}

struct MetadataSearchSheet: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav
    let game: Game
    @Bindable var search: MetadataSearch
    @FocusState private var fieldFocused: Bool

    var body: some View {
        let sources = hub.metadata.activeSources.map(\.displayName).formatted(.list(type: .or))
        SheetScaffold(title: "Find metadata", subtitle: "Match \(game.id.rawValue) on \(sources)", escapeCloses: true) {
            VStack(alignment: .leading, spacing: 16) {
                searchField

                if let error = search.error {
                    Label(error, systemImage: "exclamationmark.triangle.fill")
                        .font(.system(size: 13))
                        .foregroundStyle(Theme.accent)
                        .padding(.horizontal, 22)
                }

                ScrollViewReader { proxy in
                    ScrollView {
                        LazyVStack(spacing: 8) {
                            if let results = search.results, results.isEmpty {
                                Text("No matches. Try a shorter title, or add IGDB and SteamGridDB keys in Settings.")
                                    .font(.system(size: 14))
                                    .foregroundStyle(Theme.tertiaryText)
                                    .frame(maxWidth: .infinity, alignment: .leading)
                            }
                            ForEach(Array((search.results ?? []).enumerated()), id: \.element.id) { index, candidate in
                                let target = Navigator.Target.metadataResult(index)
                                Button { nav.select(target) } label: { row(candidate, focused: nav.isFocused(target)) }
                                    .buttonStyle(.plain)
                                    .onHover { if $0 { nav.hover(target) } }
                                    .disabled(search.applying != nil)
                                    .id(target)
                            }
                        }
                        .padding(.horizontal, 22)
                        .padding(.bottom, 22)
                    }
                    .onChange(of: nav.focus) { _, focus in
                        guard nav.focusMovedByController, case .metadataResult? = focus else { return }
                        withAnimation(Theme.focusAnimation) { proxy.scrollTo(focus, anchor: .center) }
                    }
                }
            }
        }
        .task {
            search.query = hub.displayTitle(game)
            await search.search()
        }
        .onScreenKeyboard(search.keyboard)
        .onChange(of: search.editingQuery) { _, editing in fieldFocused = editing }
        .onChange(of: fieldFocused) { _, focused in search.editingQuery = focused }
        .presentationDetents([.large])
    }

    private var searchField: some View {
        let focused = nav.isFocused(.metadataQuery)
        return HStack(spacing: 10) {
            Image(systemName: "magnifyingglass").foregroundStyle(Theme.tertiaryText)
            TextField("Title", text: $search.query)
                .textFieldStyle(.plain)
                .font(.system(size: 17))
                .focused($fieldFocused)
                .onSubmit { Task { await search.search() } }
                .autocorrectionDisabled()
            if search.searching { ProgressView().controlSize(.small) }
        }
        .padding(14)
        .background(focused ? Color.white.opacity(0.16) : Theme.surface, in: .rect(cornerRadius: 12, style: .continuous))
        .overlay {
            RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(focused ? .white : Theme.stroke, lineWidth: focused ? 2 : 1)
        }
        .animation(Theme.focusAnimation, value: focused)
        .onHover { if $0 { nav.hover(.metadataQuery) } }
        .padding(.horizontal, 22)
    }

    private func row(_ candidate: MetadataCandidate, focused: Bool) -> some View {
        HStack(spacing: 14) {
            AsyncImage(url: candidate.thumbnail) { image in
                image.resizable().aspectRatio(contentMode: .fill)
            } placeholder: {
                PlaceholderArt(title: candidate.title)
            }
            .frame(width: 48, height: 72)
            .clipShape(.rect(cornerRadius: 6, style: .continuous))
            VStack(alignment: .leading, spacing: 4) {
                Text(candidate.title).font(.system(size: 16, weight: .semibold)).lineLimit(2)
                HStack(spacing: 8) {
                    Tag(text: candidate.ref.source.displayName)
                    if let year = candidate.year { Text(year).foregroundStyle(Theme.secondaryText) }
                }
                .font(.system(size: 13))
            }
            Spacer()
            if search.applying == candidate.id { ProgressView() } else { Image(systemName: "chevron.right").foregroundStyle(Theme.tertiaryText) }
        }
        .padding(12)
        .background(focused ? Color.white.opacity(0.16) : Theme.surface, in: .rect(cornerRadius: 12, style: .continuous))
        .overlay {
            RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(focused ? .white : .clear, lineWidth: 2)
        }
        .animation(Theme.focusAnimation, value: focused)
        .contentShape(.rect)
    }
}

// MARK: Confirmation

/// A question asked before a change the player might not mean, such as removing a library folder or deleting a game's
/// folder. Cancel has the focus at first, so a stray press of A changes nothing.
final class Confirmation {
    let id = UUID()
    let title: String
    let message: String
    /// The button that goes ahead, such as "Remove folder".
    let action: String
    /// Draws that button in red, for a change that removes or deletes something.
    let destructive: Bool
    /// The game it's about, so the sheet closes when that game is hidden or leaves the library.
    let gameID: GameID?
    let perform: @MainActor () async -> Void

    init(title: String, message: String, action: String, destructive: Bool, gameID: GameID? = nil,
         perform: @escaping @MainActor () async -> Void) {
        self.title = title
        self.message = message
        self.action = action
        self.destructive = destructive
        self.gameID = gameID
        self.perform = perform
    }
}

struct ConfirmationSheet: View {
    let confirmation: Confirmation

    var body: some View {
        SheetScaffold(title: confirmation.title, escapeCloses: true) {
            VStack(alignment: .leading, spacing: 22) {
                // Scrolls, so a long path never pushes the buttons off a small screen.
                ScrollView {
                    Text(confirmation.message)
                        .font(.system(size: 15))
                        .foregroundStyle(.white.opacity(0.86))
                        .fixedSize(horizontal: false, vertical: true)
                        .textSelection(.enabled)
                        .frame(maxWidth: .infinity, alignment: .leading)
                }
                .scrollBounceBehavior(.basedOnSize)
                HStack(spacing: 12) {
                    HubButton(target: .confirmAction, kind: confirmation.destructive ? .destructive : .primary) { Text(confirmation.action) }
                    HubButton(target: .confirmCancel) { Text("Cancel") }
                }
            }
            .padding(.horizontal, 22)
            .padding(.bottom, 22)
        }
        .presentationDetents([.medium, .large])
        #if os(macOS)
        .frame(minWidth: 480, minHeight: 280)
        #endif
    }
}
