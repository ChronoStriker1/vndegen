import Foundation
#if os(iOS)
import UIKit
#endif

/// How the home screen draws the library. The top bar's view button steps through them, and each device keeps its own.
enum LibraryLayout: String, CaseIterable {
    /// The focused game's hero, logo, and Play button above shelves of capsules.
    case featured
    /// Shelves of large capsules with no hero, so the capsule art fills the screen.
    case capsules
    /// Header art stacked in a list, in as many columns as the width allows.
    case headers

    static let storageKey = "libraryLayout"

    /// The stored choice, or the one that suits this device: headers on iPhone, capsules on iPad, featured on Mac.
    static var setting: LibraryLayout {
        UserDefaults.standard.string(forKey: storageKey).flatMap(Self.init(rawValue:)) ?? deviceDefault
    }

    private static var deviceDefault: LibraryLayout {
        #if os(iOS)
        UIDevice.current.userInterfaceIdiom == .phone ? .headers : .capsules
        #else
        .featured
        #endif
    }

    var next: LibraryLayout {
        let all = Self.allCases
        return all[(all.firstIndex(of: self)! + 1) % all.count]
    }

    var title: String {
        switch self {
        case .featured: "Featured"
        case .capsules: "Capsule rows"
        case .headers: "Header list"
        }
    }

    var systemImage: String {
        switch self {
        case .featured: "play.rectangle.fill"
        case .capsules: "rectangle.split.3x1.fill"
        case .headers: "rectangle.grid.1x2.fill"
        }
    }
}
