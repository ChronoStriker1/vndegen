import Metal
import QuartzCore
import VNCore
#if canImport(UIKit)
import UIKit
public typealias FrameHostBaseView = UIView
#else
import AppKit
public typealias FrameHostBaseView = NSView
#endif

/// Shows a frame a runtime drew on the CPU, letterboxed to the view's shape, through Metal. For the runtimes that
/// composite in software and hand the host RGBA pixels: VNDegen's NScripter, RGSS, System 3, System 4 and Wolf RPG
/// runtimes. It also reports the player's input on the view in engine-neutral terms.
public final class FrameView: FrameHostBaseView {
    public enum Filter: Sendable { case nearest, linear }

    /// How the frame is scaled to the view. Pixel art reads better with nearest.
    public var filter: Filter = .linear { didSet { redraw() } }
    /// Called once, when the view is first in a window at a usable size.
    public var onReady: (() -> Void)?
    /// The mouse, a touch or a trackpad, as pointer events normalized over the frame (not the view), clamped to it.
    public var onPointer: ((PointerEvent) -> Void)?
    /// Scrolling, in notches: positive is away from the player.
    public var onScroll: ((Double) -> Void)?
    /// A hardware key, as its USB HID usage, going down or up. `characters` is what a key going down typed.
    public var onKey: ((_ hidUsage: Int, _ pressed: Bool, _ characters: String?) -> Void)?

    private let device = MTLCreateSystemDefaultDevice()
    private var queue: MTLCommandQueue?
    private var pipeline: MTLRenderPipelineState?
    private var samplers: [Filter: MTLSamplerState] = [:]
    /// The frame on screen. Frames go into three textures in turn, so a new frame never overwrites one the GPU may
    /// still be drawing; `lastUse` holds the last command buffer that drew each.
    private var texture: MTLTexture? { textures.isEmpty ? nil : textures[current] }
    private var textures: [MTLTexture] = []
    private var lastUse: [MTLCommandBuffer?] = []
    private var current = 0
    private var metalLayer: CAMetalLayer { layer as! CAMetalLayer }

    public init() {
        super.init(frame: .zero)
        #if canImport(AppKit) && !canImport(UIKit)
        wantsLayer = true
        #endif
        setUpMetal()
        #if canImport(UIKit)
        backgroundColor = .black
        isMultipleTouchEnabled = false
        #endif
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) { fatalError("not used") }

    #if canImport(UIKit)
    override public class var layerClass: AnyClass { CAMetalLayer.self }
    #else
    override public func makeBackingLayer() -> CALayer { CAMetalLayer() }
    override public var isFlipped: Bool { true }
    #endif

    private func setUpMetal() {
        guard let device else { return }
        let layer = metalLayer
        layer.device = device
        layer.pixelFormat = .bgra8Unorm
        layer.framebufferOnly = true
        layer.isOpaque = true
        queue = device.makeCommandQueue()
        do {
            let library = try device.makeLibrary(source: Self.shaderSource, options: nil)
            let desc = MTLRenderPipelineDescriptor()
            desc.vertexFunction = library.makeFunction(name: "frame_vertex")
            desc.fragmentFunction = library.makeFunction(name: "frame_fragment")
            desc.colorAttachments[0].pixelFormat = .bgra8Unorm
            pipeline = try device.makeRenderPipelineState(descriptor: desc)
        } catch {
            frameHostLog.error("Metal setup failed: \(error.localizedDescription, privacy: .public)")
        }
        for filter in [Filter.nearest, .linear] {
            let desc = MTLSamplerDescriptor()
            desc.minFilter = filter == .nearest ? .nearest : .linear
            desc.magFilter = desc.minFilter
            desc.sAddressMode = .clampToEdge
            desc.tAddressMode = .clampToEdge
            samplers[filter] = device.makeSamplerState(descriptor: desc)
        }
    }

    /// Uploads a frame of RGBA8 pixels, rows packed, and draws it.
    public func present(_ pixels: UnsafeRawPointer, width: Int, height: Int) {
        guard let device, width > 0, height > 0 else { return }
        if texture?.width != width || texture?.height != height {
            let desc = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .rgba8Unorm, width: width, height: height, mipmapped: false)
            desc.usage = .shaderRead
            textures = (0..<3).compactMap { _ in device.makeTexture(descriptor: desc) }
            lastUse = Array(repeating: nil, count: textures.count)
            current = 0
        } else {
            current = (current + 1) % textures.count
        }
        guard current < textures.count else { return }
        if let buffer = lastUse[current], buffer.status != .completed, buffer.status != .error { buffer.waitUntilCompleted() }
        textures[current].replace(region: MTLRegionMake2D(0, 0, width, height), mipmapLevel: 0, withBytes: pixels, bytesPerRow: width * 4)
        redraw()
    }

    /// The frame's rectangle in the view, in points: as large as fits, centred.
    public var contentRect: CGRect {
        guard let texture, bounds.width > 0, bounds.height > 0 else { return bounds }
        let scale = min(bounds.width / CGFloat(texture.width), bounds.height / CGFloat(texture.height))
        let size = CGSize(width: CGFloat(texture.width) * scale, height: CGFloat(texture.height) * scale)
        return CGRect(x: (bounds.width - size.width) / 2, y: (bounds.height - size.height) / 2, width: size.width, height: size.height)
    }

    /// A point normalized over the view (as the hub's pointer events are) mapped into the frame, normalized and
    /// clamped to it.
    public func framePoint(fromViewNormalized p: CGPoint) -> CGPoint {
        framePoint(CGPoint(x: p.x * bounds.width, y: p.y * bounds.height))
    }

    func framePoint(_ viewPoint: CGPoint) -> CGPoint {
        let r = contentRect
        guard r.width > 0, r.height > 0 else { return .zero }
        return CGPoint(x: min(max((viewPoint.x - r.minX) / r.width, 0), 1), y: min(max((viewPoint.y - r.minY) / r.height, 0), 1))
    }

    public func redraw() {
        guard let queue, let pipeline, let texture, bounds.width > 0, bounds.height > 0 else { return }
        let scale = contentsScale
        let drawableSize = CGSize(width: bounds.width * scale, height: bounds.height * scale)
        if metalLayer.drawableSize != drawableSize { metalLayer.drawableSize = drawableSize }
        guard let drawable = metalLayer.nextDrawable(), let buffer = queue.makeCommandBuffer() else { return }
        let pass = MTLRenderPassDescriptor()
        pass.colorAttachments[0].texture = drawable.texture
        pass.colorAttachments[0].loadAction = .clear
        pass.colorAttachments[0].clearColor = MTLClearColor(red: 0, green: 0, blue: 0, alpha: 1)
        pass.colorAttachments[0].storeAction = .store
        guard let encoder = buffer.makeRenderCommandEncoder(descriptor: pass) else { return }
        let r = contentRect
        encoder.setViewport(MTLViewport(originX: Double(r.minX * scale), originY: Double(r.minY * scale),
                                        width: Double(r.width * scale), height: Double(r.height * scale), znear: 0, zfar: 1))
        encoder.setRenderPipelineState(pipeline)
        encoder.setFragmentTexture(texture, index: 0)
        encoder.setFragmentSamplerState(samplers[filter], index: 0)
        encoder.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: 4)
        encoder.endEncoding()
        buffer.present(drawable)
        buffer.commit()
        lastUse[current] = buffer
    }

    private var contentsScale: CGFloat {
        #if canImport(UIKit)
        window?.screen.scale ?? traitCollection.displayScale
        #else
        window?.backingScaleFactor ?? 2
        #endif
    }

    private func readyIfOnScreen() {
        metalLayer.contentsScale = contentsScale
        guard window != nil, bounds.width >= 1, bounds.height >= 1, let onReady else { return }
        self.onReady = nil
        // Start after this layout pass, so the runtime's first frame comes with the view's final size.
        RunLoop.main.perform(inModes: [.common]) { MainActor.assumeIsolated(onReady) }
    }

    private static let shaderSource = """
    #include <metal_stdlib>
    using namespace metal;
    struct VOut { float4 position [[position]]; float2 uv; };
    vertex VOut frame_vertex(uint id [[vertex_id]]) {
        float2 uv = float2((id & 1) ? 1.0 : 0.0, (id & 2) ? 1.0 : 0.0);
        VOut out;
        out.position = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
        out.uv = uv;
        return out;
    }
    fragment float4 frame_fragment(VOut in [[stage_in]], texture2d<float> frame [[texture(0)]], sampler s [[sampler(0)]]) {
        return float4(frame.sample(s, in.uv).rgb, 1.0);
    }
    """

    // MARK: Platform events

    #if canImport(UIKit)
    override public func didMoveToWindow() {
        super.didMoveToWindow()
        readyIfOnScreen()
        takeFocus()
    }

    override public func layoutSubviews() {
        super.layoutSubviews()
        readyIfOnScreen()
        redraw()
    }

    public func takeFocus() {
        if window != nil { becomeFirstResponder() }
    }

    override public var canBecomeFirstResponder: Bool { true }

    override public func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) { touch(touches, .began) }
    override public func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) { touch(touches, .moved) }
    override public func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) { touch(touches, .ended) }
    override public func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) { touch(touches, .cancelled) }

    private func touch(_ touches: Set<UITouch>, _ phase: PointerEvent.Phase) {
        guard let t = touches.first else { return }
        onPointer?(PointerEvent(phase: phase, location: framePoint(t.location(in: self))))
    }

    override public func pressesBegan(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        let rest = forward(presses, pressed: true)
        if !rest.isEmpty { super.pressesBegan(rest, with: event) }
    }

    override public func pressesEnded(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        let rest = forward(presses, pressed: false)
        if !rest.isEmpty { super.pressesEnded(rest, with: event) }
    }

    override public func pressesCancelled(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        let rest = forward(presses, pressed: false)
        if !rest.isEmpty { super.pressesCancelled(rest, with: event) }
    }

    /// UIKit's key codes are USB HID usages already. Returns the presses that aren't keys.
    private func forward(_ presses: Set<UIPress>, pressed: Bool) -> Set<UIPress> {
        presses.filter { press in
            guard let key = press.key else { return true }
            let typed = pressed && !key.modifierFlags.contains(.command) ? key.characters : nil
            onKey?(key.keyCode.rawValue, pressed, typed)
            return false
        }
    }
    #else
    override public func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        readyIfOnScreen()
        takeFocus()
    }

    override public func setFrameSize(_ newSize: NSSize) {
        super.setFrameSize(newSize)
        readyIfOnScreen()
        redraw()
    }

    override public func viewDidChangeBackingProperties() {
        super.viewDidChangeBackingProperties()
        metalLayer.contentsScale = contentsScale
        redraw()
    }

    public func takeFocus() { window?.makeFirstResponder(self) }

    override public var acceptsFirstResponder: Bool { true }
    override public func acceptsFirstMouse(for event: NSEvent?) -> Bool { true }

    override public func updateTrackingAreas() {
        super.updateTrackingAreas()
        trackingAreas.forEach(removeTrackingArea)
        addTrackingArea(NSTrackingArea(rect: .zero, options: [.mouseMoved, .activeInKeyWindow, .inVisibleRect], owner: self))
    }

    override public func mouseDown(with event: NSEvent) { pointer(.began, event, .primary) }
    override public func mouseDragged(with event: NSEvent) { pointer(.moved, event, .primary) }
    override public func mouseUp(with event: NSEvent) { pointer(.ended, event, .primary) }
    override public func mouseMoved(with event: NSEvent) { pointer(.moved, event, .primary) }
    override public func rightMouseDown(with event: NSEvent) { pointer(.began, event, .secondary) }
    override public func rightMouseDragged(with event: NSEvent) { pointer(.moved, event, .secondary) }
    override public func rightMouseUp(with event: NSEvent) { pointer(.ended, event, .secondary) }

    /// A mouse wheel reports lines, one to a notch; a trackpad reports points, about ten to a notch. Both report how
    /// content should move, positive for down, so the sign flips.
    override public func scrollWheel(with event: NSEvent) {
        let notches = event.hasPreciseScrollingDeltas ? event.scrollingDeltaY / 10 : event.scrollingDeltaY
        onScroll?(notches)
    }

    override public func keyDown(with event: NSEvent) {
        guard let usage = MacKeyCodes.hidUsage(event.keyCode) else { return }
        onKey?(usage, true, event.modifierFlags.contains(.command) ? nil : event.characters)
    }

    override public func keyUp(with event: NSEvent) {
        guard let usage = MacKeyCodes.hidUsage(event.keyCode) else { return }
        onKey?(usage, false, nil)
    }

    private var heldModifiers: Set<UInt16> = []

    override public func flagsChanged(with event: NSEvent) {
        guard let usage = MacKeyCodes.hidUsage(event.keyCode), usage != 57 else { return }  // not Caps Lock
        let pressed = heldModifiers.insert(event.keyCode).inserted
        if !pressed { heldModifiers.remove(event.keyCode) }
        onKey?(usage, pressed, nil)
    }

    private func pointer(_ phase: PointerEvent.Phase, _ event: NSEvent, _ button: PointerEvent.Button) {
        let point = convert(event.locationInWindow, from: nil)
        onPointer?(PointerEvent(phase: phase, location: framePoint(point), button: button))
    }
    #endif
}
