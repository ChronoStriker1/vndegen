import SwiftUI

extension EnvironmentValues {
    /// How far a sheet's top-trailing button moves in from the trailing edge, to keep out of the column of a camera in
    /// that corner, such as the iPhone Duo's on its outer display. Zero elsewhere. The hub sets it for its sheets.
    @Entry public var cameraCornerTrailing: CGFloat = 0
}
