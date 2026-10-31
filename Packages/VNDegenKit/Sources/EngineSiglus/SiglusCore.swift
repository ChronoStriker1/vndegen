import CSiglus
import Darwin
import Foundation

/// The C table VNDegen's interface to siglus_rs exports (`vnsiglus.h`), found at run time in the VNSiglus framework
/// the app embeds, so the package builds and tests without it.
nonisolated struct SiglusCore: @unchecked Sendable {
    let api: VNSiglusApiV1

    /// Nil when this build of VNDegen has no engine, or an older one with a smaller table.
    static let shared: SiglusCore? = {
        // RTLD_DEFAULT, a C macro Swift cannot import.
        let everywhere = UnsafeMutableRawPointer(bitPattern: -2)
        guard let symbol = dlsym(everywhere, "vnsiglus_get_api_v1") else { return nil }
        let getAPI = unsafeBitCast(symbol, to: VNSiglusGetApiV1Fn.self)
        guard let table = getAPI(), table.pointee.struct_size >= MemoryLayout<VNSiglusApiV1>.size else { return nil }
        return SiglusCore(api: table.pointee)
    }()

    /// Sets the engines' log file and the font that stands in for MS PGothic, the font pack's substitute.
    func prepare(log: URL, font: URL?) {
        api.set_log_file!(log.path)
        if let font, !Self.fontSet {
            Self.fontSet = api.set_default_font!(font.path) == 1
        }
    }

    /// The engine keeps the first default font it gets for the life of the process.
    nonisolated(unsafe) private static var fontSet = false

    /// A message the engine returned, which it allocated.
    func take(_ error: UnsafeMutablePointer<CChar>?) -> String {
        guard let error else { return "unknown error" }
        defer { api.string_free!(error) }
        return String(cString: error)
    }
}
