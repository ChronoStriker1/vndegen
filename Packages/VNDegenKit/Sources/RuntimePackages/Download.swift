import CryptoKit
import Foundation
import Synchronization
import VNCore

/// Downloads a package to a file, hashing as bytes arrive, and refuses anything but the pinned size and SHA-256.
enum PackageDownload {
    static func fetch(_ package: RuntimePackage, to file: URL, session: URLSession,
                      progress: @escaping @Sendable (RuntimeInstallProgress) -> Void) async throws {
        guard FileManager.default.createFile(atPath: file.path, contents: nil) else {
            throw CocoaError(.fileWriteUnknown, userInfo: [NSFilePathErrorKey: file.path])
        }
        let receiver = Receiver(output: try FileHandle(forWritingTo: file), expectedSize: package.size, progress: progress)
        let task = session.dataTask(with: URLRequest(url: package.download))
        task.delegate = receiver
        let digest = try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { continuation in
                receiver.start(task, continuation: continuation)
            }
        } onCancel: {
            task.cancel()
        }
        try Task.checkCancellation()
        progress(.verifying)
        let actual = digest.map { String(format: "%02x", $0) }.joined()
        guard actual == package.sha256.lowercased() else {
            throw RuntimeInstallError.checksumMismatch(expected: package.sha256, actual: actual)
        }
    }

    /// Receives one data task's callbacks and hands the result to the waiting install.
    private final class Receiver: NSObject, URLSessionDataDelegate, Sendable {
        private struct State {
            var output: FileHandle?
            var hash = SHA256()
            var received: Int64 = 0
            var reportedPercent = -1
            var failure: (any Error)?
            var continuation: CheckedContinuation<SHA256Digest, any Error>?
        }

        private let state: Mutex<State>
        private let expectedSize: Int64
        private let progress: @Sendable (RuntimeInstallProgress) -> Void

        init(output: FileHandle, expectedSize: Int64, progress: @escaping @Sendable (RuntimeInstallProgress) -> Void) {
            state = Mutex(State(output: output))
            self.expectedSize = expectedSize
            self.progress = progress
        }

        func start(_ task: URLSessionDataTask, continuation: CheckedContinuation<SHA256Digest, any Error>) {
            // A task cancelled before this point has already run its cancellation handler on a task not yet started.
            guard !Task.isCancelled else { return continuation.resume(throwing: CancellationError()) }
            state.withLock { $0.continuation = continuation }
            task.resume()
        }

        func urlSession(_ session: URLSession, dataTask: URLSessionDataTask, didReceive response: URLResponse)
            async -> URLSession.ResponseDisposition {
            if let status = (response as? HTTPURLResponse)?.statusCode, !(200..<300).contains(status) {
                state.withLock { $0.failure = RuntimeInstallError.httpStatus(status) }
                return .cancel
            }
            return .allow
        }

        func urlSession(_ session: URLSession, dataTask: URLSessionDataTask, didReceive data: Data) {
            enum Next { case wait, report(Int64), stop }
            let next: Next = state.withLock { state in
                guard state.failure == nil, let output = state.output else { return .wait }
                state.received += Int64(data.count)
                guard state.received <= expectedSize else {
                    state.failure = RuntimeInstallError.sizeMismatch(expected: expectedSize, actual: state.received)
                    return .stop
                }
                do {
                    try output.write(contentsOf: data)
                } catch {
                    state.failure = error
                    return .stop
                }
                state.hash.update(data: data)
                let percent = Int(state.received * 100 / max(expectedSize, 1))
                guard percent > state.reportedPercent else { return .wait }
                state.reportedPercent = percent
                return .report(state.received)
            }
            switch next {
            case .wait: break
            case .report(let received): progress(.downloading(received: received, total: expectedSize))
            case .stop: dataTask.cancel()
            }
        }

        func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: (any Error)?) {
            state.withLock { state in
                try? state.output?.close()
                state.output = nil
                guard let continuation = state.continuation else { return }
                state.continuation = nil
                if let failure = state.failure {
                    continuation.resume(throwing: failure)
                } else if let error {
                    // A cancelled transfer that no check here stopped was cancelled by its task.
                    let cancelled = (error as? URLError)?.code == .cancelled
                    continuation.resume(throwing: cancelled ? CancellationError() : error)
                } else if state.received != expectedSize {
                    continuation.resume(throwing: RuntimeInstallError.sizeMismatch(expected: expectedSize, actual: state.received))
                } else {
                    continuation.resume(returning: state.hash.finalize())
                }
            }
        }
    }
}
