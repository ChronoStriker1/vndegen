import Foundation

@MainActor public final class NotificationCount {
    public private(set) var count = 0
    private var observer: (any NSObjectProtocol)?

    public init(_ name: Notification.Name, object: AnyObject) {
        observer = NotificationCenter.default.addObserver(forName: name, object: object, queue: nil) { [weak self] _ in
            MainActor.assumeIsolated { self?.count += 1 }
        }
    }
}
