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
    private let halos = NSCache<NSString, HaloEntry>()

    /// NSCache can't hold nil, and "this logo needs no halo" is the common answer worth remembering.
    private final class HaloEntry {
        let halo: LogoHalo?
        init(_ halo: LogoHalo?) { self.halo = halo }
    }

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

    func cachedHalo(_ url: URL) -> LogoHalo?? { halos.object(forKey: url.path as NSString).map(\.halo) }

    /// The glow for a logo, worked out once per file. The halo is blurred, so one small render serves every display size.
    func halo(_ url: URL) async -> LogoHalo? {
        if let hit = cachedHalo(url) { return hit }
        let halo = await Task.detached(priority: .userInitiated) { Self.makeHalo(url) }.value
        halos.setObject(HaloEntry(halo), forKey: url.path as NSString)
        return halo
    }

    /// Builds a halo when enough of the logo is dark ink that it would sink into the hero's dark scrim. Strength grows
    /// with the share of dark pixels, so outlined logos get a faint glow and black ink logos a clear one. Thresholds come
    /// from measuring the demo library's Steam logos: OMORI has no dark pixels, Steins;Gate about half.
    nonisolated private static func makeHalo(_ url: URL) -> LogoHalo? {
        guard let source = CGImageSourceCreateWithURL(url as CFURL, [kCGImageSourceShouldCache: false] as CFDictionary),
              let logo = CGImageSourceCreateThumbnailAtIndex(source, 0, [
                  kCGImageSourceCreateThumbnailFromImageAlways: true,
                  kCGImageSourceCreateThumbnailWithTransform: true,
                  kCGImageSourceThumbnailMaxPixelSize: 256,
              ] as CFDictionary)
        else { return nil }
        let width = logo.width, height = logo.height
        guard let context = CGContext(data: nil, width: width, height: height, bitsPerComponent: 8, bytesPerRow: width * 4,
                                      space: CGColorSpace(name: CGColorSpace.sRGB)!,
                                      bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue),
              let data = context.data
        else { return nil }
        context.draw(logo, in: CGRect(x: 0, y: 0, width: width, height: height))
        let pixels = data.bindMemory(to: UInt8.self, capacity: width * height * 4)
        var ink = 0.0, darkInk = 0.0
        for offset in stride(from: 0, to: width * height * 4, by: 4) {
            let alpha = Double(pixels[offset + 3])
            guard alpha > 0 else { continue }
            // Channels are premultiplied, so dividing by alpha recovers the color's own luma.
            let luma = (0.2126 * Double(pixels[offset]) + 0.7152 * Double(pixels[offset + 1]) + 0.0722 * Double(pixels[offset + 2])) / alpha
            ink += alpha
            if luma < 0.3 { darkInk += alpha }
        }
        // A logo without transparency is a picture, not ink, and a halo would only outline its rectangle.
        guard ink > 0, ink / (255 * Double(width * height)) < 0.9 else { return nil }
        let strength = min(1, (darkInk / ink - 0.3) / 0.4)
        guard strength > 0.05 else { return nil }

        let longSide = CGFloat(max(width, height))
        let sigma = longSide * 0.05, spread = (longSide * 0.015).rounded()
        let pad = (sigma * 3 + spread).rounded(.up)
        let input = CIImage(cgImage: logo)
        let silhouette = CIImage(color: .white).cropped(to: input.extent)
            .applyingFilter("CIBlendWithAlphaMask", parameters: [kCIInputBackgroundImageKey: CIImage.empty(), kCIInputMaskImageKey: input])
        let glow = silhouette
            .applyingFilter("CIMorphologyMaximum", parameters: [kCIInputRadiusKey: spread])
            .applyingGaussianBlur(sigma: sigma)
        let canvas = input.extent.insetBy(dx: -pad, dy: -pad)
        guard let image = blurContext.createCGImage(glow, from: canvas) else { return nil }
        return LogoHalo(image: image, inset: pad / CGFloat(width), opacity: 0.75 * strength)
    }

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

/// A white glow shaped like a logo, larger than the logo by `inset` times the logo's width on every side.
nonisolated struct LogoHalo: Sendable {
    let image: CGImage
    let inset: CGFloat
    let opacity: Double
}

/// A local artwork file drawn into a frame of `size` points, or `placeholder` while it loads or when it is missing.
struct ArtworkImage<Placeholder: View>: View {
    @Environment(\.displayScale) private var displayScale
    let url: URL?
    let size: CGSize
    var contentMode: ContentMode = .fill
    var alignment: Alignment = .center
    var blurred = false
    /// Draws a soft light glow behind the image when it is a dark logo. See `ArtworkCache.halo`.
    var halo = false
    @ViewBuilder var placeholder: Placeholder
    @State private var image: CGImage?
    @State private var logoHalo: LogoHalo?

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
                    .background {
                        if let logoHalo {
                            GeometryReader { geo in
                                Image(decorative: logoHalo.image, scale: 1)
                                    .resizable()
                                    .padding(-logoHalo.inset * geo.size.width)
                                    .opacity(logoHalo.opacity)
                            }
                        }
                    }
            } else {
                placeholder
            }
        }
        .frame(width: size.width, height: size.height, alignment: alignment)
        .task(id: LoadKey(url: url, rendering: rendering)) {
            guard let url else { image = nil; logoHalo = nil; return }
            let rendering = rendering
            let cache = ArtworkCache.shared
            if let hit = cache.cached(url, rendering) { image = hit } else { image = await cache.image(url, rendering) }
            guard halo else { return }
            if let hit = cache.cachedHalo(url) { logoHalo = hit } else { logoHalo = await cache.halo(url) }
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
        ArtworkImage(url: hub.artworkURL(game, [.logo]), size: maxSize, contentMode: .fit, alignment: alignment, halo: true) {
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
