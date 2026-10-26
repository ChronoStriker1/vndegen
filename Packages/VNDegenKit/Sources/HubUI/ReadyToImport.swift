import SwiftUI
import ArchiveImport

/// The archives in VNDegen's folder, one under another, for the empty library.
struct ReadyToImportList: View {
    let archives: [LocalArchive]

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            SectionLabel(text: "Ready to import", systemImage: "archivebox")
            ForEach(archives) { ArchiveCard(archive: $0) }
        }
    }
}

/// The archives in VNDegen's folder as a shelf, for the library home.
struct ReadyToImportRow: View {
    @Environment(\.hubMetrics) private var metrics
    let archives: [LocalArchive]

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            SectionLabel(text: "Ready to import", systemImage: "archivebox")
                .padding(.leading, metrics.leading)
            ScrollView(.horizontal) {
                LazyHStack(spacing: metrics.tileSpacing) {
                    ForEach(archives) { archive in
                        // On a phone the next card peeks in, so a second archive is easy to spot.
                        ArchiveCard(archive: archive, width: metrics.isCompact ? (metrics.size.width - metrics.padding * 2) * 0.88 : 380)
                    }
                }
                .padding(.leading, metrics.leading)
                .padding(.trailing, metrics.trailing)
                // Room for the focused card's scale and glow.
                .padding(.vertical, 10)
            }
            .scrollIndicators(.hidden)
            .scrollClipDisabled()
        }
        .id(Navigator.archivesRowID)
    }
}

/// One archive with its Import action. Activating the card opens the import sheet with the archive chosen.
private struct ArchiveCard: View {
    @Environment(Navigator.self) private var nav
    let archive: LocalArchive
    var width: CGFloat?

    var body: some View {
        let target = Navigator.Target.importArchive(archive.url)
        Button { nav.select(target) } label: {
            HStack(spacing: 14) {
                Image(systemName: "archivebox.fill")
                    .font(.system(size: 20, weight: .semibold))
                    .foregroundStyle(Theme.accent)
                    .frame(width: 44, height: 44)
                    .background(Theme.accent.opacity(0.16), in: .rect(cornerRadius: 10, style: .continuous))
                VStack(alignment: .leading, spacing: 3) {
                    Text(archive.fileName)
                        .font(.system(size: 15, weight: .semibold))
                        .lineLimit(2)
                        .multilineTextAlignment(.leading)
                    Text(detail)
                        .font(.system(size: 13))
                        .foregroundStyle(Theme.secondaryText)
                }
                Spacer(minLength: 8)
                // Keeps its full width, so a long file name wraps instead of the button's label.
                Label("Import", systemImage: "square.and.arrow.down")
                    .font(.system(size: 14, weight: .bold))
                    .lineLimit(1)
                    .fixedSize()
                    .padding(.horizontal, 14)
                    .frame(minHeight: 36)
                    .background(Theme.accent.gradient, in: .capsule)
            }
            .padding(12)
            .frame(width: width)
            .frame(maxWidth: width == nil ? .infinity : nil, alignment: .leading)
        }
        .buttonStyle(ArchiveCardStyle(focused: nav.isFocused(target)))
        .onHover { if $0 { nav.hover(target) } }
        .id(target)
        .accessibilityLabel("Import \(archive.fileName)")
    }

    private var detail: String {
        let size = archive.byteCount.formatted(.byteCount(style: .file))
        return archive.parts > 1 ? "\(size) · \(archive.parts) parts" : size
    }
}

private struct ArchiveCardStyle: ButtonStyle {
    let focused: Bool

    func makeBody(configuration: Configuration) -> some View {
        let shape = RoundedRectangle(cornerRadius: 14, style: .continuous)
        configuration.label
            .background(focused ? Color.white.opacity(0.14) : Theme.surface, in: shape)
            .overlay { shape.strokeBorder(focused ? .white : Theme.stroke, lineWidth: focused ? 2.5 : 1) }
            .contentShape(shape)
            .scaleEffect(configuration.isPressed ? 0.97 : (focused ? 1.03 : 1))
            .shadow(color: focused ? Theme.accent.opacity(0.45) : .clear, radius: 16)
            .animation(Theme.focusAnimation, value: focused)
            .animation(Theme.focusAnimation, value: configuration.isPressed)
    }
}
