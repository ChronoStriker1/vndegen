import SwiftUI
import VNCore
#if os(iOS)
import UIKit
import os
#endif

extension PlayOrientation {
    static let storageKey = "inGameOrientation"

    /// The Settings value. Landscape until the user changes it.
    static var setting: PlayOrientation {
        UserDefaults.standard.string(forKey: storageKey).flatMap(Self.init(rawValue:)) ?? .landscape
    }

    var title: String {
        switch self {
        case .landscape: "Landscape"
        case .portrait: "Portrait"
        case .followDevice: "Follow device"
        }
    }
}

#if os(iOS)
/// Reports the orientations the app allows right now. Install it in the app with `@UIApplicationDelegateAdaptor`.
public final class HubAppDelegate: NSObject, UIApplicationDelegate {
    public func application(_ application: UIApplication, supportedInterfaceOrientationsFor window: UIWindow?) -> UIInterfaceOrientationMask {
        OrientationLock.mask
    }
}

/// Locks the interface orientation while a game plays and releases it in the hub.
enum OrientationLock {
    /// The hub's own rotation, the same as Info.plist.
    static var hubMask: UIInterfaceOrientationMask {
        UIDevice.current.userInterfaceIdiom == .pad ? .all : .allButUpsideDown
    }
    private(set) static var mask = hubMask

    private static var deviceMask: UIInterfaceOrientationMask? {
        switch UIDevice.current.orientation {
        case .portrait: .portrait
        case .portraitUpsideDown: hubMask.contains(.portraitUpsideDown) ? .portraitUpsideDown : nil
        // Device and interface landscape are mirrored: the device turned left shows the interface's right side up.
        case .landscapeLeft: .landscapeRight
        case .landscapeRight: .landscapeLeft
        default: nil
        }
    }

    /// The interface orientation from before the game, for when the device cannot say how it is held.
    private static var resumeMask: UIInterfaceOrientationMask?

    private static let startDeviceOrientation: Void = UIDevice.current.beginGeneratingDeviceOrientationNotifications()

    /// iPadOS refuses programmatic rotation in its windowing modes. From iPadOS 26 a game instead locks the orientation
    /// it is shown in, through `SceneOrientationLock`, and the system keeps that lock only while the app fills the screen.
    static var usesSceneLock: Bool {
        guard #available(iOS 26, *) else { return false }
        return UIDevice.current.userInterfaceIdiom == .pad
    }

    /// Nil restores the hub's rotation.
    static func apply(_ orientation: PlayOrientation?) {
        guard !usesSceneLock else { return }
        _ = startDeviceOrientation
        let scenes = UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }
        let request: UIInterfaceOrientationMask
        switch orientation {
        case .landscape?: (mask, request) = (.landscape, .landscape)
        case .portrait?: (mask, request) = (.portrait, .portrait)
        case .followDevice?: (mask, request) = (hubMask, deviceMask ?? hubMask)
        case nil:
            // Allowing every orientation again does not rotate a landscape-locked screen back, so ask for the way the
            // device is held, or failing that (face up, or a simulator) the way the hub was shown before the game.
            (mask, request) = (hubMask, deviceMask ?? resumeMask ?? hubMask)
        }
        if orientation == nil {
            resumeMask = nil
        } else if resumeMask == nil {
            resumeMask = scenes.first.flatMap { mask(for: $0.effectiveGeometry.interfaceOrientation) }
        }
        for scene in scenes {
            for window in scene.windows { window.rootViewController?.setNeedsUpdateOfSupportedInterfaceOrientations() }
            // iPadOS ignores this for scenes that support multitasking, and reports it here.
            scene.requestGeometryUpdate(.iOS(interfaceOrientations: request)) { error in
                Logger(subsystem: "com.chronostriker1.vndegen", category: "hub")
                    .error("orientation request refused: \(error.localizedDescription, privacy: .public)")
            }
        }
    }

    private static func mask(for orientation: UIInterfaceOrientation) -> UIInterfaceOrientationMask? {
        switch orientation {
        case .portrait: .portrait
        case .portraitUpsideDown: .portraitUpsideDown
        case .landscapeLeft: .landscapeLeft
        case .landscapeRight: .landscapeRight
        default: nil
        }
    }
}

/// Locks the scene in the game's orientation once the scene shows it, and reports when a full-screen scene still has
/// to be turned. iPadOS cannot be told to rotate, only to hold the current orientation.
@available(iOS 26, *)
struct SceneOrientationLock: UIViewControllerRepresentable {
    let orientation: PlayOrientation
    @Binding var needsRotation: Bool

    func makeUIViewController(context: Context) -> Controller { Controller() }

    func updateUIViewController(_ controller: Controller, context: Context) {
        controller.wanted = orientation
        controller.onNeedsRotation = { needsRotation = $0 }
        controller.update()
    }

    final class Controller: UIViewController {
        var wanted = PlayOrientation.followDevice
        var onNeedsRotation: (Bool) -> Void = { _ in }
        private var locked = false
        private var needsRotation = false

        override var prefersInterfaceOrientationLocked: Bool { locked }

        override func viewDidLayoutSubviews() {
            super.viewDidLayoutSubviews()
            update()
        }

        override func viewWillDisappear(_ animated: Bool) {
            super.viewWillDisappear(animated)
            setLocked(false)
        }

        func update() {
            guard let window = view.window, let scene = window.windowScene else { return }
            let size = window.bounds.size
            let isLandscape = size.width > size.height
            let shows = switch wanted {
            case .landscape: isLandscape
            case .portrait: !isLandscape
            case .followDevice: true
            }
            setLocked(wanted != .followDevice && shows)
            // In a window the game follows the window, so only a full-screen scene asks to be turned.
            let rotate = !shows && size == scene.screen.bounds.size
            if rotate != needsRotation {
                needsRotation = rotate
                // Layout runs inside SwiftUI's update, which must not change state.
                Task { @MainActor [onNeedsRotation] in onNeedsRotation(rotate) }
            }
        }

        private func setLocked(_ lock: Bool) {
            guard lock != locked else { return }
            locked = lock
            setNeedsUpdateOfPrefersInterfaceOrientationLocked()
            #if DEBUG
            Task { @MainActor [weak scene = view.window?.windowScene] in
                try? await Task.sleep(for: .milliseconds(300))
                guard let geometry = scene?.effectiveGeometry else { return }
                Logger(subsystem: "com.chronostriker1.vndegen", category: "hub").notice(
                    "ORIENTATION prefersLocked=\(lock) isInterfaceOrientationLocked=\(geometry.isInterfaceOrientationLocked) interface=\(geometry.interfaceOrientation.rawValue)")
            }
            #endif
        }
    }
}
#endif
