import Foundation

/// Names the Keychain services VNDegen keeps its secrets under: the hidden games' passcode, API keys, and the save
/// share's password. A DEBUG run with `-libraryRoot` sets `testSuffix` first thing at launch, as it moves its settings to
/// a suite of their own, so a test run never reads or changes the installed app's items. Empty otherwise.
nonisolated public enum KeychainService {
    nonisolated(unsafe) public static var testSuffix = ""

    public static func named(_ base: String) -> String { base + testSuffix }
}
