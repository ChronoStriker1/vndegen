import SwiftUI
import ImageIO
import CoreImage
import VNCore

/// How a piece of artwork is drawn, which decides the pixels worth decoding.
nonisolated enum ArtworkRendering: Hashable, Sendable {
    /// Scaled to fill or fit a frame of this many pixels.
    case fill(width: Int, height: Int)
    case fit(width: Int, height: Int)
    /// A soft wash for backgrounds. Blurred once from a small thumbnail, then scaled up by the GPU.
    case blurred
}

/// Decoded artwork keyed by file URL and rendering. Steam heroes are 3840 pixels wide, so everything is decoded at the
/// size it is drawn, off the main thread, and blurred copies are made once per image instead of blurring every frame.
/// LibraryStore writes a new file name per change, so URL keys never go stale.
@MainActor
final class ArtworkCache {
    static let shared = ArtworkCache()
    private let cache = NSCache<NSString, CGImage>()

    private func key(_ url: URL, _ rendering: ArtworkRendering) -> NSString { "\(url.path)#\(rendering)" as NSString }

    func cached(_ url: URL, _ rendering: ArtworkRendering) -> CGImage? { cache.object(forKey: key(url, rendering)) }

    func image(_ url: URL, _ rendering: ArtworkRendering) async -> CGImage? {
        if let hit = cached(url, rendering) { return hit }
        let image = await Task.detached(priority: .userInitiated) { Self.decode(url, rendering) }.value
        if let image { cache.setObject(image, forKey: key(url, rendering)) }
        return image
    }

    /// A wide hero filling a tall frame is decoded at full height, but only its middle is visible. Copying out the
    /// visible part keeps a 3840 pixel wide texture from reaching the GPU for a phone-width slice.
    nonisolated private static func cropToVisible(_ image: CGImage, width: Int, height: Int) -> CGImage {
        let scale = max(CGFloat(width) / CGFloat(image.width), CGFloat(height) / CGFloat(image.height))
        let visible = CGSize(width: min(CGFloat(image.width), CGFloat(width) / scale),
                             height: min(CGFloat(image.height), CGFloat(height) / scale))
        guard visible.width * visible.height < CGFloat(image.width * image.height) * 0.75 else { return image }
        let rect = CGRect(x: ((CGFloat(image.width) - visible.width) / 2).rounded(.down),
                          y: ((CGFloat(image.height) - visible.height) / 2).rounded(.down),
                          width: visible.width.rounded(.down), height: visible.height.rounded(.down))
        guard let cropped = image.cropping(to: rect),
              let context = CGContext(data: nil, width: cropped.width, height: cropped.height, bitsPerComponent: 8, bytesPerRow: 0,
                                      space: CGColorSpace(name: CGColorSpace.sRGB)!,
                                      bitmapInfo: CGImageAlphaInfo.premultipliedFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue)
        else { return image }
        // Drawing into a new bitmap drops the reference to the full-size backing store that cropping(to:) keeps.
        context.draw(cropped, in: CGRect(x: 0, y: 0, width: cropped.width, height: cropped.height))
        return context.makeImage() ?? image
    }

    nonisolated private static let blurContext = CIContext(options: [.cacheIntermediates: false])

    nonisolated private static func decode(_ url: URL, _ rendering: ArtworkRendering) -> CGImage? {
        guard let source = CGImageSourceCreateWithURL(url as CFURL, [kCGImageSourceShouldCache: false] as CFDictionary),
              let properties = CGImageSourceCopyPropertiesAtIndex(source, 0, nil) as? [CFString: Any],
              let sourceWidth = properties[kCGImagePropertyPixelWidth] as? CGFloat,
              let sourceHeight = properties[kCGImagePropertyPixelHeight] as? CGFloat,
              sourceWidth > 0, sourceHeight > 0 else { return nil }
        let scale: CGFloat
        switch rendering {
        case let .fill(width, height): scale = max(CGFloat(width) / sourceWidth, CGFloat(height) / sourceHeight)
        case let .fit(width, height): scale = min(CGFloat(width) / sourceWidth, CGFloat(height) / sourceHeight)
        case .blurred: scale = 160 / max(sourceWidth, sourceHeight)
        }
        let maxPixel = max(1, Int((max(sourceWidth, sourceHeight) * min(scale, 1)).rounded(.up)))
        let options: [CFString: Any] = [
            kCGImageSourceCreateThumbnailFromImageAlways: true,
            kCGImageSourceCreateThumbnailWithTransform: true,
            kCGImageSourceShouldCacheImmediately: true,
            kCGImageSourceThumbnailMaxPixelSize: maxPixel,
        ]
        guard let image = CGImageSourceCreateThumbnailAtIndex(source, 0, options as CFDictionary) else { return nil }
        if case let .fill(width, height) = rendering { return cropToVisible(image, width: width, height: height) }
        guard rendering == .blurred else { return image }
        let input = CIImage(cgImage: image)
        let blurred = input.clampedToExtent().applyingGaussianBlur(sigma: 7).cropped(to: input.extent)
        return blurContext.createCGImage(blurred, from: input.extent)
    }
}

/// A local artwork file drawn into a frame of `size` points, or `placeholder` while it loads or when it is missing.
struct ArtworkImage<Placeholder: View>: View {
    @Environment(\.displayScale) private var displayScale
    let url: URL?
    let size: CGSize
    var contentMode: ContentMode = .fill
    var alignment: Alignment = .center
    var blurred = false
    @ViewBuilder var placeholder: Placeholder
    @State private var image: CGImage?

    /// Pixel sizes snap to 64 so small layout changes reuse the cached decode.
    private var rendering: ArtworkRendering {
        if blurred { return .blurred }
        let width = Int((size.width * displayScale / 64).rounded(.up)) * 64
        let height = Int((size.height * displayScale / 64).rounded(.up)) * 64
        return contentMode == .fill ? .fill(width: width, height: height) : .fit(width: width, height: height)
    }

    private struct LoadKey: Hashable {
        let url: URL?
        let rendering: ArtworkRendering
    }

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
        .frame(width: size.width, height: size.height, alignment: alignment)
        .task(id: LoadKey(url: url, rendering: rendering)) {
            guard let url else { image = nil; return }
            let rendering = rendering
            if let hit = ArtworkCache.shared.cached(url, rendering) { image = hit; return }
            image = await ArtworkCache.shared.image(url, rendering)
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

/// The game's art for `slots` in a frame of `size`, or a generated poster when there is none.
struct GameArt: View {
    @Environment(Hub.self) private var hub
    let game: Game
    let slots: [ArtworkSlot]
    let size: CGSize
    var blurred = false

    var body: some View {
        ArtworkImage(url: hub.artworkURL(game, slots), size: size, blurred: blurred) {
            PlaceholderArt(title: hub.displayTitle(game), wide: slots.first != .capsule)
        }
        .clipped()
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
        ArtworkImage(url: hub.artworkURL(game, [.logo]), size: maxSize, contentMode: .fit, alignment: alignment) {
            Text(title)
                .font(.display(maxSize.height * 0.42, weight: .black))
                .lineLimit(2)
                .minimumScaleFactor(0.4)
                .multilineTextAlignment(alignment == .center ? .center : .leading)
                .foregroundStyle(.white)
                .shadow(color: .black.opacity(0.6), radius: 12, y: 4)
                .frame(maxWidth: maxSize.width, maxHeight: maxSize.height, alignment: alignment)
        }
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
