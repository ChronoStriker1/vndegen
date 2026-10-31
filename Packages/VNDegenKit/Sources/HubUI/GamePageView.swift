import SwiftUI
import VNCore
import GameInput

struct GamePageView: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav
    @Environment(\.hubMetrics) private var metrics
    let game: Game

    var body: some View {
        ScrollViewReader { proxy in
            ScrollView(.vertical) {
                VStack(alignment: .leading, spacing: 0) {
                    banner.id("banner")
                    VStack(alignment: .leading, spacing: 28) {
                        actions
                        RuntimeSection(game: game)
                        if metrics.isCompact {
                            VStack(alignment: .leading, spacing: 28) {
                                about
                                details
                            }
                        } else {
                            HStack(alignment: .top, spacing: 40) {
                                about.frame(maxWidth: .infinity, alignment: .leading)
                                details.frame(width: min(380, metrics.size.width * 0.36))
                            }
                        }
                    }
                    .padding(.leading, metrics.leading)
                    .padding(.trailing, metrics.trailing)
                    .padding(.top, 22)
                    .padding(.bottom, 56)
                    .frame(maxWidth: 1400, alignment: .leading)
                }
            }
            .scrollIndicators(.hidden)
            .ignoresSafeArea(edges: [.horizontal, .top])
            .onChange(of: nav.focus) { _, focus in
                guard nav.focusMovedByController else { return }
                withAnimation(Theme.focusAnimation) {
                    if let focus, [.pageEngine, .pageVariant, .pageOrientation, .pageControls, .pageMapping].contains(focus) {
                        proxy.scrollTo(focus, anchor: .center)
                    } else {
                        proxy.scrollTo("banner", anchor: .top)
                    }
                }
            }
        }
        // The game's engine decides which runtime packages it needs, and an install finishing changes which are missing.
        .task(id: "\(game.id)|\(game.engine?.rawValue ?? "")") { await nav.runtimes.refresh(game.id) }
        .task(id: game.id) { await hub.metadata.fillAbout(game.id, in: hub.library) }
        .runtimeFolderImporter(nav.runtimes, game: game.id)
        .background {
            GeometryReader { geo in
                GameArt(game: game, slots: [.hero, .header, .capsule], size: geo.size, blurred: true)
                    .opacity(0.3)
            }
            .background(Theme.background)
            .ignoresSafeArea()
        }
    }

    private var banner: some View {
        GameArt(game: game, slots: [.hero, .header, .capsule],
                size: CGSize(width: metrics.fullWidth, height: metrics.insets.top + metrics.bannerHeight))
            .overlay {
                LinearGradient(colors: [.black.opacity(0.45), .clear], startPoint: .top, endPoint: UnitPoint(x: 0.5, y: 0.3))
            }
            // Fade into the blurred page background instead of a solid color, so the banner has no bottom edge.
            .mask(LinearGradient(stops: [.init(color: .black, location: 0.72), .init(color: .clear, location: 1)],
                                 startPoint: .top, endPoint: .bottom))
            .overlay(alignment: .bottomLeading) {
                GameLogo(game: game, maxSize: metrics.logoMaxSize)
                    .padding(.leading, metrics.leading)
                    .padding(.bottom, 8)
            }
            .overlay(alignment: .topLeading) {
                HubButton(target: .back, kind: .icon) { Image(systemName: "chevron.left") }
                    .accessibilityLabel("Back")
                    .padding(.leading, metrics.insets.leading + metrics.topRowPadding(.leading))
                    .padding(.top, metrics.insets.top + 10)
                    #if os(macOS)
                    .padding(.top, 18)
                    #endif
            }
    }

    private var actions: some View {
        VStack(alignment: .leading, spacing: 16) {
            ViewThatFits(in: .horizontal) {
                HStack(spacing: 12) {
                    playButton
                    Spacer().frame(width: 4)
                    secondaryButtons(labels: true)
                }
                .onAppear { nav.pageActionsStacked = false }
                HStack(spacing: 10) {
                    playButton
                    Spacer().frame(width: 4)
                    secondaryButtons(labels: false)
                }
                .onAppear { nav.pageActionsStacked = false }
                // The narrowest windows put the other buttons under Play, and controller focus moves down to them.
                VStack(alignment: .leading, spacing: 12) {
                    playButton
                    HStack(spacing: 10) { secondaryButtons(labels: false) }
                }
                .onAppear { nav.pageActionsStacked = true }
            }
            HeroFacts(game: game)
        }
    }

    private var playButton: some View {
        HubButton(target: .pagePlay, kind: .primary) { Label("Play", systemImage: "play.fill") }
    }

    @ViewBuilder
    private func secondaryButtons(labels: Bool) -> some View {
        HubButton(target: .pageArtwork, kind: labels ? .secondary : .icon) {
            labeled("Artwork", "photo.on.rectangle", labels)
        }
        HubButton(target: .pageMetadata, kind: labels ? .secondary : .icon) {
            labeled("Find metadata", "text.magnifyingglass", labels)
        }
        HubButton(target: .pageSaves, kind: labels ? .secondary : .icon) {
            labeled("Saves", "externaldrive", labels)
        }
        HubButton(target: .pageLock, kind: labels ? .secondary : .icon) {
            labeled(game.isLocked ? "Hidden" : "Hide", game.isLocked ? "lock.fill" : "lock.open", labels)
        }
        .accessibilityValue(game.isLocked ? "Hidden" : "Visible")
    }

    @ViewBuilder
    private func labeled(_ title: String, _ image: String, _ showTitle: Bool) -> some View {
        if showTitle { Label(title, systemImage: image) } else { Image(systemName: image).accessibilityLabel(title) }
    }

    @ViewBuilder
    private var about: some View {
        let metadata = game.metadata
        VStack(alignment: .leading, spacing: 14) {
            SectionLabel(text: "About")
            if let summary = metadata?.summary, !summary.isEmpty {
                Text(summary)
                    .font(.system(size: 16))
                    .lineSpacing(4)
                    .foregroundStyle(.white.opacity(0.86))
                    .fixedSize(horizontal: false, vertical: true)
            } else {
                Text("No description yet. Use Find metadata to match this game on VNDB, IGDB, or Steam.")
                    .font(.system(size: 15))
                    .foregroundStyle(Theme.tertiaryText)
            }
            if let genres = metadata?.genres, !genres.isEmpty {
                FlowTags(tags: genres)
            }
            if let screenshots = metadata?.screenshots, !screenshots.isEmpty {
                ScreenshotRow(screenshots: screenshots)
            }
        }
    }

    private var details: some View {
        let metadata = game.metadata
        return VStack(alignment: .leading, spacing: 0) {
            InfoRow(label: "Developer", value: metadata?.developers.joined(separator: ", "))
            InfoRow(label: "Publisher", value: metadata?.publishers.joined(separator: ", "))
            InfoRow(label: "Released", value: metadata?.releaseDate.map(Self.formatRelease))
            InfoRow(label: "Play time", value: game.playTime.playTimeText ?? "None")
            InfoRow(label: "Folder", value: game.id.rawValue)
            EngineSection(game: game)
                .padding(.top, 14)
            #if os(iOS)
            OrientationSection(game: game)
                .padding(.top, 18)
            #endif
            ControlsSection(game: game)
                .padding(.top, 18)
            if let cheats = game.cheats, cheats.isActive {
                CheatsSection(settings: cheats)
                    .padding(.top, 18)
            }
        }
        .padding(18)
        .background(Theme.surface, in: .rect(cornerRadius: 16, style: .continuous))
        .overlay { RoundedRectangle(cornerRadius: 16, style: .continuous).strokeBorder(Theme.stroke) }
    }

    /// Metadata dates may be partial ("2024" or "2024-03").
    static func formatRelease(_ iso: String) -> String {
        let parts = iso.split(separator: "-").compactMap { Int($0) }
        var components = DateComponents()
        components.year = parts.first
        components.month = parts.count > 1 ? parts[1] : nil
        components.day = parts.count > 2 ? parts[2] : nil
        guard let date = Calendar(identifier: .gregorian).date(from: components) else { return iso }
        switch parts.count {
        case 1: return iso
        case 2: return date.formatted(.dateTime.month(.wide).year())
        default: return date.formatted(date: .long, time: .omitted)
        }
    }
}

private struct InfoRow: View {
    let label: String
    let value: String?

    var body: some View {
        if let value, !value.isEmpty {
            HStack(alignment: .firstTextBaseline) {
                Text(label).foregroundStyle(Theme.tertiaryText)
                Spacer(minLength: 16)
                Text(value).foregroundStyle(.white).multilineTextAlignment(.trailing)
            }
            .font(.system(size: 14))
            .padding(.vertical, 9)
            .overlay(alignment: .bottom) { Rectangle().fill(Theme.stroke).frame(height: 1) }
        }
    }
}

/// The game's in-game orientation and the button that opens its override.
private struct OrientationSection: View {
    let game: Game

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            SectionLabel(text: "In-game orientation", systemImage: "rotate.right")
            HStack(spacing: 12) {
                VStack(alignment: .leading, spacing: 4) {
                    Text((game.orientation ?? PlayOrientation.setting).title)
                        .font(.system(size: 17, weight: .bold))
                    Text(game.orientation == nil ? "Default from Settings" : "Set for this game")
                        .font(.system(size: 13))
                        .foregroundStyle(Theme.secondaryText)
                }
                Spacer()
                HubButton(target: .pageOrientation, kind: .secondary) { Text("Change") }
            }
        }
    }
}

/// The cheats that stay on for this game. They change in the Cheats page of the in-game menu.
private struct CheatsSection: View {
    let settings: CheatSettings

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            SectionLabel(text: "Cheats", systemImage: "wand.and.stars")
                .padding(.bottom, 6)
            Text("On: " + settings.summary.joined(separator: ", "))
                .font(.system(size: 17, weight: .bold))
                .fixedSize(horizontal: false, vertical: true)
            Text("Change them in the game's Cheats menu, in the VNDegen menu while it plays.")
                .font(.system(size: 13))
                .foregroundStyle(Theme.secondaryText)
        }
    }
}

/// The game's control method on a touch device, its button mapping, and the buttons that change them.
private struct ControlsSection: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav
    let game: Game

    var body: some View {
        let mapping = ButtonMapping(for: game, in: hub.library)
        let remapped = mapping.changes(from: ButtonMapping.defaultLayout(for: game, in: hub.library))
        VStack(alignment: .leading, spacing: 10) {
            SectionLabel(text: "Controls", systemImage: "gamecontroller")
            if nav.offersControlMethod(game.id) {
                HStack(spacing: 12) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text((game.controlMethod ?? ControlMethod.setting).title)
                            .font(.system(size: 17, weight: .bold))
                        Text(game.controlMethod == nil ? "Default from Settings" : "Set for this game")
                            .font(.system(size: 13))
                            .foregroundStyle(Theme.secondaryText)
                    }
                    Spacer()
                    HubButton(target: .pageControls, kind: .secondary) { Text("Change") }
                }
            }
            HStack(spacing: 12) {
                VStack(alignment: .leading, spacing: 4) {
                    Text("Button mapping")
                        .font(.system(size: 17, weight: .bold))
                    Text(ButtonMappingEditor.Layout.named(mapping)?.title
                         ?? (remapped == 1 ? "1 button remapped" : "\(remapped) buttons remapped"))
                        .font(.system(size: 13))
                        .foregroundStyle(Theme.secondaryText)
                }
                Spacer()
                HubButton(target: .pageMapping, kind: .secondary) { Text("Edit") }
            }
        }
    }
}

/// The engine in use, how it was chosen, and the button that opens the manual override. Below them, what the engine
/// says about running this game here, and its advanced options.
private struct EngineSection: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav
    let game: Game
    @State private var note: String?

    var body: some View {
        let plugin = hub.library.plugin(for: game.engine)
        VStack(alignment: .leading, spacing: 10) {
            SectionLabel(text: "Engine", systemImage: "cpu")
            HStack(alignment: .center, spacing: 12) {
                VStack(alignment: .leading, spacing: 4) {
                    Text(plugin?.descriptor.displayName ?? "No engine")
                        .font(.system(size: 17, weight: .bold))
                        .foregroundStyle(plugin == nil ? Theme.accent : .white)
                    Text(game.engineOverride == nil ? "Automatic" : "Chosen manually")
                        .font(.system(size: 13))
                        .foregroundStyle(Theme.secondaryText)
                }
                Spacer()
                HubButton(target: .pageEngine, kind: .secondary) { Text("Change") }
            }
            if let detected = game.detected {
                let name = hub.library.plugin(for: detected.engine)?.descriptor.displayName ?? detected.engine.rawValue
                VStack(alignment: .leading, spacing: 4) {
                    Text("Detected \(name), \(Self.confidence(detected.confidence)) match\(detected.version.map { ", version \($0)" } ?? "")")
                    Text(detected.evidence)
                        .font(.system(size: 12, design: .monospaced))
                        .foregroundStyle(.white.opacity(0.8))
                        .padding(.horizontal, 8)
                        .padding(.vertical, 4)
                        .background(.black.opacity(0.35), in: .rect(cornerRadius: 6))
                }
                .font(.system(size: 13))
                .foregroundStyle(Theme.secondaryText)
            } else {
                Text("No engine recognized this folder. Pick one to try it anyway.")
                    .font(.system(size: 13))
                    .foregroundStyle(Theme.secondaryText)
            }
            if let note {
                Text(note)
                    .font(.system(size: 13))
                    .foregroundStyle(Theme.secondaryText)
                    .fixedSize(horizontal: false, vertical: true)
            }
            if let variants = nav.variants(game.id) {
                HStack(alignment: .center, spacing: 12) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(variants.title).font(.system(size: 15, weight: .semibold))
                        Text(variants.options.first { $0.id == variants.selected }?.title ?? "Automatic, \(variants.automatic)")
                            .font(.system(size: 13))
                            .foregroundStyle(Theme.secondaryText)
                    }
                    Spacer()
                    HubButton(target: .pageVariant, kind: .secondary) { Text("Change") }
                }
                .padding(.top, 4)
            }
        }
        // The note can read the game's folder, so it is worked out off the main thread. It follows the engine choice,
        // the engine's options, and what the last play recorded once that play has finished closing.
        .task(id: "\(game.engine?.rawValue ?? "")|\(hub.closing.contains(game.id))|\(nav.variantRevision)") {
            let folder = hub.library.paths.folder(for: game.id), data = hub.library.paths.dataDirectory(for: game.id)
            note = await Task.detached { [game] in plugin?.compatibilityNote(for: game, folder: folder, dataDirectory: data) }.value
        }
    }

    static func confidence(_ value: DetectionConfidence) -> String {
        switch value {
        case .weak: "weak"
        case .likely: "likely"
        case .certain: "certain"
        }
    }
}

/// The game's screenshots in a row that scrolls sideways.
private struct ScreenshotRow: View {
    let screenshots: [Screenshot]

    var body: some View {
        ScrollView(.horizontal) {
            LazyHStack(spacing: 10) {
                ForEach(screenshots, id: \.url) { screenshot in
                    AsyncImage(url: screenshot.thumbnail) { image in
                        image.resizable().aspectRatio(contentMode: .fill)
                    } placeholder: {
                        Theme.surface
                    }
                    .frame(width: 256, height: 144)
                    .clipShape(.rect(cornerRadius: 8, style: .continuous))
                    .overlay { RoundedRectangle(cornerRadius: 8, style: .continuous).strokeBorder(Theme.stroke) }
                }
            }
        }
        .scrollIndicators(.hidden)
        .accessibilityLabel("Screenshots")
    }
}

private struct FlowTags: View {
    let tags: [String]

    var body: some View {
        FlowLayout(spacing: 8) {
            ForEach(tags, id: \.self) { Tag(text: $0) }
        }
    }
}

/// Wraps children onto new lines, for genre tags.
struct FlowLayout: Layout {
    var spacing: CGFloat

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        let rows = arrange(subviews, width: proposal.width ?? .infinity)
        return CGSize(width: rows.map(\.width).max() ?? 0, height: rows.last.map { $0.y + $0.height } ?? 0)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        for row in arrange(subviews, width: bounds.width) {
            var x = bounds.minX
            for index in row.indices {
                let size = subviews[index].sizeThatFits(.unspecified)
                subviews[index].place(at: CGPoint(x: x, y: bounds.minY + row.y), proposal: .unspecified)
                x += size.width + spacing
            }
        }
    }

    private struct Row { var indices: [Int] = []; var width: CGFloat = 0; var height: CGFloat = 0; var y: CGFloat = 0 }

    private func arrange(_ subviews: Subviews, width: CGFloat) -> [Row] {
        var rows = [Row()]
        for index in subviews.indices {
            let size = subviews[index].sizeThatFits(.unspecified)
            if !rows[rows.count - 1].indices.isEmpty, rows[rows.count - 1].width + spacing + size.width > width {
                let last = rows[rows.count - 1]
                rows.append(Row(y: last.y + last.height + spacing))
            }
            var row = rows[rows.count - 1]
            row.width += (row.indices.isEmpty ? 0 : spacing) + size.width
            row.height = max(row.height, size.height)
            row.indices.append(index)
            rows[rows.count - 1] = row
        }
        return rows
    }
}
