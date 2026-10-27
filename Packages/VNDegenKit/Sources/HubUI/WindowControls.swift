import SwiftUI
#if os(iOS)
import UIKit
#endif

/// The room an iPad window's controls take in its top corners, from iPadOS 26, in the window's own coordinates. Zero
/// where there are none: full screen, iPhone, and Mac.
struct WindowControlsInsets: Equatable {
    /// How far from the window's leading and trailing edges a view in the top row starts, so it sits beside the controls.
    var leading: CGFloat = 0
    var trailing: CGFloat = 0
    /// How far from the window's top a view in a top corner starts, so it sits below the controls.
    var top: CGFloat = 0
}

#if os(iOS)
/// Reports `WindowControlsInsets` for the window it fills. Place it behind the content, ignoring the safe area.
struct WindowControlsReader: UIViewRepresentable {
    let onChange: (WindowControlsInsets) -> Void

    func makeUIView(context: Context) -> Probe {
        let probe = Probe()
        probe.isUserInteractionEnabled = false
        probe.onChange = onChange
        return probe
    }

    func updateUIView(_ probe: Probe, context: Context) { probe.onChange = onChange }

    final class Probe: UIView {
        var onChange: (WindowControlsInsets) -> Void = { _ in }
        private var reported = WindowControlsInsets()

        override func layoutSubviews() {
            super.layoutSubviews()
            report()
        }

        override func safeAreaInsetsDidChange() {
            super.safeAreaInsetsDidChange()
            report()
        }

        private func report() {
            guard #available(iOS 26, *), window != nil else { return }
            // The corner-adapted safe area grows on one axis to clear the controls. Apple's guidance for a top bar is
            // the horizontal one, which keeps the bar's height and moves its ends.
            let beside = directionalEdgeInsets(for: .safeArea(cornerAdaptation: .horizontal))
            let below = directionalEdgeInsets(for: .safeArea(cornerAdaptation: .vertical))
            let insets = WindowControlsInsets(leading: beside.leading, trailing: beside.trailing, top: below.top)
            guard insets != reported else { return }
            reported = insets
            // Layout runs inside SwiftUI's update, which must not change state.
            Task { @MainActor [onChange] in onChange(insets) }
        }
    }
}
#endif
