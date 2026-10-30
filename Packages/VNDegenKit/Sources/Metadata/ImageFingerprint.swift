import CoreGraphics
import Foundation
import ImageIO

/// A 64 × 64 color copy of an image, for spotting one picture hosted by two sources. Resizing and re-encoding move
/// almost no pixels far, while another caption or color scheme moves a whole region.
struct ImageFingerprint: Sendable {
    static let side = 64
    /// A pixel differs when one of its channels moves by more than this.
    static let channelTolerance = 40
    /// Two images are the same picture when at most this share of their pixels differ. Copies on Steam, IGDB, VNDB, and
    /// SteamGridDB measured 0.000. A SteamGridDB logo that only changed its caption measured 0.017.
    static let maxDifferingShare = 0.005

    /// RGB, row by row, over mid gray so transparent logos compare by their shapes.
    let pixels: [UInt8]
    /// Width over height. A crop or another layout of the same art is a different picture.
    let aspect: Double

    init?(_ data: Data) {
        // The transform applies EXIF orientation, so the copy and its shape are what the picker shows.
        guard let source = CGImageSourceCreateWithData(data as CFData, nil),
              let image = CGImageSourceCreateThumbnailAtIndex(source, 0, [
                  kCGImageSourceCreateThumbnailFromImageAlways: true,
                  kCGImageSourceCreateThumbnailWithTransform: true,
                  kCGImageSourceThumbnailMaxPixelSize: 256,
              ] as CFDictionary),
              image.width > 0, image.height > 0
        else { return nil }
        let side = Self.side
        var rgba = [UInt8](repeating: 0, count: side * side * 4)
        let drawn = rgba.withUnsafeMutableBytes { buffer -> Bool in
            guard let context = CGContext(data: buffer.baseAddress, width: side, height: side, bitsPerComponent: 8,
                                          bytesPerRow: side * 4, space: CGColorSpaceCreateDeviceRGB(),
                                          bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)
            else { return false }
            context.interpolationQuality = .high
            context.setFillColor(gray: 0.5, alpha: 1)
            context.fill(CGRect(x: 0, y: 0, width: side, height: side))
            context.draw(image, in: CGRect(x: 0, y: 0, width: side, height: side))
            return true
        }
        guard drawn else { return nil }
        pixels = rgba.enumerated().filter { $0.offset % 4 != 3 }.map(\.element)
        aspect = Double(image.width) / Double(image.height)
    }

    /// The share of pixels that differ, or nil when the shapes differ.
    func difference(_ other: ImageFingerprint) -> Double? {
        guard abs(aspect / other.aspect - 1) < 0.04 else { return nil }
        var differing = 0
        for pixel in stride(from: 0, to: pixels.count, by: 3) {
            let moved = (0..<3).contains { abs(Int(pixels[pixel + $0]) - Int(other.pixels[pixel + $0])) > Self.channelTolerance }
            if moved { differing += 1 }
        }
        return Double(differing) / Double(Self.side * Self.side)
    }

    func matches(_ other: ImageFingerprint) -> Bool {
        difference(other).map { $0 <= Self.maxDifferingShare } ?? false
    }
}
