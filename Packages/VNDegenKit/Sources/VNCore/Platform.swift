#if canImport(UIKit)
import UIKit
public typealias PlatformView = UIView
public typealias PlatformImage = UIImage
#else
import AppKit
public typealias PlatformView = NSView
public typealias PlatformImage = NSImage
#endif
