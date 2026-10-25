import SwiftUI
import ImageIO
import VNCore

/// Decoded, downsampled artwork keyed by file URL and pixel size. Steam heroes are 3840 pixels wide, so decoding at
/// display size keeps memory flat. LibraryStore writes a new file name per change, so URL keys never go stale.
@MainActor
final class ArtworkCache {
    static let shared = ArtworkCache()
    private let cache = NSCache<NSString, CGImage>()

    private func key(_ url: URL, _ maxPixel: Int) -> NSString { "\(url.path)#\(maxPixel)" as NSString }

    func cached(_ url: URL, maxPixel: Int) -> CGImage? { cache.object(forKey: key(url, maxPixel)) }

    func image(_ url: URL, maxPixel: Int) async -> CGImage? {
        if let hit = cached(url, maxPixel: maxPixel) { return hit }
        let image = await Task.detached(priority: .userInitiated) { Self.downsample(url, maxPixel: maxPixel) }.value
        if let image { cache.setObject(image, forKey: key(url, maxPixel)) }
        return image
    }

    nonisolated private static func downsample(_ url: URL, maxPixel: Int) -> CGImage? {
        guard let source = CGImageSourceCreateWithURL(url as CFURL, [kCGImageSourceShouldCache: false] as CFDictionary) else {
            return nil
        }
        let options: [CFString: Any] = [
            kCGImageSourceCreateThumbnailFromImageAlways: true,
            kCGImageSourceCreateThumbnailWithTransform: true,
            kCGImageSourceShouldCacheImmediately: true,
            kCGImageSourceThumbnailMaxPixelSize: maxPixel,
        ]
        return CGImageSourceCreateThumbnailAtIndex(source, 0, options as CFDictionary)
    }
}

/// A local artwork file, or `placeholder` while it loads or when it is missing.
struct ArtworkImage<Placeholder: View>: View {
    let url: URL?
    var maxPixel = 1200
    var contentMode: ContentMode = .fill
    @ViewBuilder var placeholder: Placeholder
    @State private var image: CGImage?

    var body: some View {
        ZStack {
            if let image, url != nil {
                Image(decorative: image, scale: 1)
                    .resizable()
                    .aspectRatio(contentMode: contentMode)
            } else {
                placeholder
            }
        }
        .task(id: url) {
            guard let url else { image = nil; return }
            if let hit = ArtworkCache.shared.cached(url, maxPixel: maxPixel) { image = hit; return }
            image = await ArtworkCache.shared.image(url, maxPixel: maxPixel)
        }
    }
}

extension Hub {
    func displayTitle(_ game: Game) -> String { game.metadata?.title ?? game.title }

    /// The first slot in `slots` that has art, so a hero can fall back to the header or capsule.
    func artworkURL(_ game: Game, _ slots: [ArtworkSlot]) -> URL? {
        slots.lazy.compactMap { slot in game.artwork[slot].flatMap { _ in self.library.artworkURL(slot, for: game.id) } }.first
    }
}

/// Fills its frame with the game's art for `slots`, or a generated poster when there is none.
struct GameArt: View {
    @Environment(Hub.self) private var hub
    let game: Game
    let slots: [ArtworkSlot]
    var maxPixel = 1200

    var body: some View {
        ArtworkImage(url: hub.artworkURL(game, slots), maxPixel: maxPixel) {
            PlaceholderArt(title: hub.displayTitle(game), wide: slots.first != .capsule)
        }
    }
}

/// The game's logo art, or its title set in the display face.
struct GameLogo: View {
    @Environment(Hub.self) private var hub
    let game: Game
    var maxSize: CGSize
    var alignment: Alignment = .bottomLeading

    var body: some View {
        let title = hub.displayTitle(game)
        ArtworkImage(url: hub.artworkURL(game, [.logo]), maxPixel: 1000, contentMode: .fit) {
            Text(title)
                .font(.display(maxSize.height * 0.42, weight: .black))
                .lineLimit(2)
                .minimumScaleFactor(0.4)
                .multilineTextAlignment(alignment == .center ? .center : .leading)
                .foregroundStyle(.white)
                .shadow(color: .black.opacity(0.6), radius: 12, y: 4)
                .frame(maxWidth: maxSize.width, maxHeight: maxSize.height, alignment: alignment)
        }
        .frame(maxWidth: maxSize.width, maxHeight: maxSize.height, alignment: alignment)
        .shadow(color: .black.opacity(0.45), radius: 18, y: 6)
        .accessibilityLabel(title)
    }
}

/// A poster generated from the title, so a game without art still looks intentional instead of grey.
struct PlaceholderArt: View {
    let title: String
    var wide = false

    var body: some View {
        let palette = Palette(title)
        GeometryReader { geo in
            let unit = wide ? geo.size.height : geo.size.width
            ZStack(alignment: .bottomLeading) {
                LinearGradient(colors: [palette.light, palette.dark], startPoint: .topLeading, endPoint: .bottomTrailing)
                RadialGradient(colors: [.white.opacity(0.22), .clear], center: .topTrailing, startRadius: 0, endRadius: unit * 1.1)
                Text(String(title.trimmingCharacters(in: .whitespaces).prefix(1)).uppercased())
                    .font(.system(size: unit * 1.15, weight: .black).width(.expanded))
                    .foregroundStyle(.white.opacity(0.09))
                    .fixedSize()
                    .offset(x: geo.size.width - unit * 0.62, y: -geo.size.height + unit * 0.98)
                if !wide {
                    VStack(alignment: .leading, spacing: unit * 0.04) {
                        Rectangle().fill(.white.opacity(0.85)).frame(width: unit * 0.16, height: max(2, unit * 0.018))
                        Text(title)
                            .font(.system(size: unit * 0.14, weight: .heavy).width(.expanded))
                            .foregroundStyle(.white)
                            .lineLimit(4)
                            .minimumScaleFactor(0.5)
                    }
                    .padding(unit * 0.09)
                }
            }
            .frame(width: geo.size.width, height: geo.size.height)
            .clipped()
        }
    }

    /// Two hues picked from a stable hash of the title, so each game keeps its colors across launches.
    struct Palette {
        let light: Color
        let dark: Color

        init(_ title: String) {
            var hash: UInt32 = 2_166_136_261
            for byte in title.utf8 { hash = (hash ^ UInt32(byte)) &* 16_777_619 }
            let hue = Double(hash % 360) / 360
            light = Color(hue: hue, saturation: 0.7, brightness: 0.62)
            dark = Color(hue: (hue + 0.09).truncatingRemainder(dividingBy: 1), saturation: 0.85, brightness: 0.2)
        }
    }
}
