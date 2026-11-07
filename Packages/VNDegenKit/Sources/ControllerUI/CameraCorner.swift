import SwiftUI

extension EnvironmentValues {
    /// How far a sheet's header keeps in from the trailing edge, to stay out of the column of a camera in that corner,
    /// such as the iPhone Duo's on its outer display. Zero elsewhere. The hub sets it for its sheets.
    ///
    /// Where it is set, sheets open full height and their Done button moves to a full-width bar at the bottom. A shorter
    /// sheet floats, and on that display its corners follow the screen's own, which differ on the camera's side, so it
    /// looked lopsided. A Done held in from the edge beside the camera looked misaligned with everything under it, and
    /// took the width a title needs. At the bottom it lines up with the content, is in reach of a thumb on a tall, narrow
    /// screen, and fills space a short sheet would leave empty.
    @Entry public var cameraCornerTrailing: CGFloat = 0
}
