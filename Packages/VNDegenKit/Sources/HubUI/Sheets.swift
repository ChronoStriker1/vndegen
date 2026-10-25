import SwiftUI
import PhotosUI
import UniformTypeIdentifiers
import VNCore
import Metadata

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
            ScrollView {
                VStack(spacing: 10) {
                    ForEach(nav.choices(for: sheet)) { choice in
                        let focused = nav.isFocused(choice.target)
                        let isSelected = selected == choice.target
                        Button { nav.select(choice.target) } label: {
                            HStack(spacing: 14) {
                                Image(systemName: isSelected ? "checkmark.circle.fill" : "circle")
                                    .font(.system(size: 20))
                                    .foregroundStyle(isSelected ? Theme.accent : Theme.tertiaryText)
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
                    }
                    Text(footnote)
                        .font(.system(size: 13))
                        .foregroundStyle(Theme.tertiaryText)
                        .padding(.top, 6)
                }
                .padding(.horizontal, 22)
                .padding(.bottom, 22)
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

/// Steam's "set custom artwork": pick a slot, then a suggested image, a photo, or a file.
struct ArtworkEditorSheet: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav
    @Environment(\.hubMetrics) private var metrics
    let game: Game

    @State private var candidates: [ArtworkCandidate] = []
    @State private var loading = false
    @State private var applying: ArtworkCandidate.ID?
    @State private var photo: PhotosPickerItem?
    @State private var importingFile = false
    @State private var error: String?

    var body: some View {
        @Bindable var nav = nav
        let slot = nav.artworkSlot
        SheetScaffold(title: "Artwork", subtitle: hub.displayTitle(game)) {
            ScrollView {
                VStack(alignment: .leading, spacing: 22) {
                    Picker("Slot", selection: $nav.artworkSlot) {
                        ForEach(ArtworkSlot.allCases, id: \.self) { Text($0.title).tag($0) }
                    }
                    .pickerStyle(.segmented)
                    .labelsHidden()

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
                        }
                    }

                    if let error {
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
        }
        .task(id: slot) { await loadCandidates(slot) }
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
        if loading {
            ProgressView().frame(maxWidth: .infinity, minHeight: 120)
        } else if candidates.isEmpty {
            Text("No suggestions for this slot. Add a SteamGridDB key in Settings, or match the game with Find metadata.")
                .font(.system(size: 14))
                .foregroundStyle(Theme.tertiaryText)
                .frame(maxWidth: .infinity, minHeight: 80, alignment: .leading)
        } else {
            let aspect = slot.nominalSize.width / slot.nominalSize.height
            let minWidth: CGFloat = aspect < 1 ? 110 : 200
            LazyVGrid(columns: [GridItem(.adaptive(minimum: minWidth), spacing: 12)], spacing: 12) {
                ForEach(candidates) { candidate in
                    Button { apply(candidate) } label: {
                        AsyncImage(url: candidate.thumbnail ?? candidate.url) { image in
                            image.resizable().aspectRatio(contentMode: slot == .logo ? .fit : .fill)
                        } placeholder: {
                            Theme.surface
                        }
                        .aspectRatio(aspect, contentMode: .fit)
                        .frame(maxWidth: .infinity)
                        .clipShape(.rect(cornerRadius: 8, style: .continuous))
                        .overlay {
                            if applying == candidate.id {
                                ZStack { Color.black.opacity(0.5); ProgressView() }
                                    .clipShape(.rect(cornerRadius: 8, style: .continuous))
                            }
                        }
                        .overlay(alignment: .bottomLeading) { Tag(text: candidate.source.displayName).padding(6) }
                    }
                    .buttonStyle(TileButtonStyle(focused: false))
                    .disabled(applying != nil)
                }
            }
        }
    }

    private func loadCandidates(_ slot: ArtworkSlot) async {
        loading = true
        candidates = await hub.metadata.artworkCandidates(for: game, slot: slot)
        loading = false
    }

    private func apply(_ candidate: ArtworkCandidate) {
        applying = candidate.id
        error = nil
        Task {
            do {
                try await hub.metadata.setArtwork(candidate, for: game.id, in: hub.library)
            } catch {
                self.error = "Couldn't download that image: \(error.localizedDescription)"
            }
            applying = nil
        }
    }

    private func importPhoto(_ item: PhotosPickerItem, slot: ArtworkSlot) async {
        defer { photo = nil }
        do {
            guard let data = try await item.loadTransferable(type: Data.self) else { return }
            let ext = item.supportedContentTypes.first?.preferredFilenameExtension ?? "jpg"
            try hub.library.setArtwork(data, fileExtension: ext, slot: slot, for: game.id)
            error = nil
        } catch {
            self.error = "Couldn't import that photo: \(error.localizedDescription)"
        }
    }

    private func importFile(_ result: Result<URL, any Error>, slot: ArtworkSlot) {
        do {
            let url = try result.get()
            let scoped = url.startAccessingSecurityScopedResource()
            defer { if scoped { url.stopAccessingSecurityScopedResource() } }
            let data = try Data(contentsOf: url)
            try hub.library.setArtwork(data, fileExtension: url.pathExtension.isEmpty ? "png" : url.pathExtension, slot: slot, for: game.id)
            error = nil
        } catch {
            self.error = "Couldn't import that file: \(error.localizedDescription)"
        }
    }
}

// MARK: Metadata search

struct MetadataSearchSheet: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav
    let game: Game

    @State private var query = ""
    @State private var results: [MetadataCandidate]?
    @State private var searching = false
    @State private var applying: MetadataCandidate.ID?
    @State private var error: String?

    var body: some View {
        let sources = hub.metadata.activeSources.map(\.displayName).formatted(.list(type: .or))
        SheetScaffold(title: "Find metadata", subtitle: "Match \(game.id.rawValue) on \(sources)", escapeCloses: true) {
            VStack(alignment: .leading, spacing: 16) {
                HStack(spacing: 10) {
                    Image(systemName: "magnifyingglass").foregroundStyle(Theme.tertiaryText)
                    TextField("Title", text: $query)
                        .textFieldStyle(.plain)
                        .font(.system(size: 17))
                        .onSubmit { Task { await search() } }
                        .autocorrectionDisabled()
                    if searching { ProgressView().controlSize(.small) }
                }
                .padding(14)
                .background(Theme.surface, in: .rect(cornerRadius: 12, style: .continuous))
                .overlay { RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(Theme.stroke) }
                .padding(.horizontal, 22)

                if let error {
                    Label(error, systemImage: "exclamationmark.triangle.fill")
                        .font(.system(size: 13))
                        .foregroundStyle(Theme.accent)
                        .padding(.horizontal, 22)
                }

                ScrollView {
                    LazyVStack(spacing: 8) {
                        if let results, results.isEmpty {
                            Text("No matches. Try a shorter title, or add IGDB and SteamGridDB keys in Settings.")
                                .font(.system(size: 14))
                                .foregroundStyle(Theme.tertiaryText)
                                .frame(maxWidth: .infinity, alignment: .leading)
                        }
                        ForEach(results ?? []) { candidate in
                            Button { apply(candidate) } label: { row(candidate) }
                                .buttonStyle(.plain)
                                .disabled(applying != nil)
                        }
                    }
                    .padding(.horizontal, 22)
                    .padding(.bottom, 22)
                }
            }
        }
        .task {
            query = hub.displayTitle(game)
            await search()
        }
        .presentationDetents([.large])
    }

    private func row(_ candidate: MetadataCandidate) -> some View {
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
            if applying == candidate.id { ProgressView() } else { Image(systemName: "chevron.right").foregroundStyle(Theme.tertiaryText) }
        }
        .padding(12)
        .background(Theme.surface, in: .rect(cornerRadius: 12, style: .continuous))
        .contentShape(.rect)
    }

    private func search() async {
        let title = query.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !title.isEmpty else { return }
        searching = true
        results = await hub.metadata.search(title)
        searching = false
    }

    private func apply(_ candidate: MetadataCandidate) {
        applying = candidate.id
        error = nil
        Task {
            do {
                try await hub.metadata.apply(candidate, to: game.id, in: hub.library)
                nav.back()
            } catch {
                self.error = "Couldn't apply \(candidate.title): \(error.localizedDescription)"
            }
            applying = nil
        }
    }
}
