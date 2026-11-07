import SwiftUI

extension EnvironmentValues {
    /// How far a sheet's top-trailing button moves in from the trailing edge, to keep out of the column of a camera in
    /// that corner, such as the iPhone Duo's on its outer display. Zero elsewhere. The hub sets it for its sheets.
    ///
    /// Where it is set, sheets open full height. A shorter sheet floats, and on that display its corners follow the
    /// screen's own, which differ on the camera's side, so it looked lopsided. Full height also puts every sheet's header
    /// beside the camera, so this one inset is right on every sheet rather than only on some.
    @Entry public var cameraCornerTrailing: CGFloat = 0
}
