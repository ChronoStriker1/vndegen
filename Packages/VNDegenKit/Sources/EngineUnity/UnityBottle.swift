import Foundation

/// The one CrossOver bottle VNDegen makes, keeps, and runs Unity games in, `<Bottles>/VNDegen` in VNDegen's own
/// Application Support. VNDegen never opens, changes, or runs anything in a bottle it didn't make.
///
/// Every CrossOver call names the bottle by its absolute path with `--scope private`. CrossOver then uses that path as
/// it is, without searching `CX_BOTTLE_PATH` and without falling back to system-wide bottles (CXBottle.pm's
/// `find_bottle` and `setup_bottle_wineprefix`). A bare name would not be safe: the player's CrossOver.conf may set
/// `CX_BOTTLE_PATH` in its `[EnvironmentVariables]`, which CrossOver applies over the environment it was started with.
/// `CX_BOTTLE_PATH` is set to the Bottles folder all the same, as a second layer. `command` refuses to build anything
/// for a bottle path that doesn't resolve into that folder.
nonisolated struct UnityBottle: Equatable, Sendable {
    static let name = "VNDegen"
    /// Unity games are 64-bit Windows 10 builds, or 32-bit ones a 64-bit bottle also runs. The option 2 proof used it.
    static let template = "win10_64"
    /// What CrossOver calls the bottle's Windows user, under `drive_c/users`.
    static let windowsUser = "crossover"

    /// The folder that holds the bottle and nothing else of the player's.
    let folder: URL
    /// CrossOver's command-line tools, `Contents/SharedSupport/CrossOver/bin` in the app.
    let tools: URL

    var path: URL { folder.appendingPathComponent(Self.name, isDirectory: true) }
    var driveC: URL { path.appendingPathComponent("drive_c", isDirectory: true) }
    var userFolder: URL { driveC.appendingPathComponent("users/\(Self.windowsUser)", isDirectory: true) }
    /// Written once setup has finished, so a bottle a crash left half made is made again.
    var readyMarker: URL { path.appendingPathComponent("vndegen-ready") }

    /// Where a game's `persistentDataPath` is in the bottle.
    func persistentData(_ info: UnityAppInfo) -> URL {
        userFolder.appendingPathComponent("AppData/LocalLow", isDirectory: true)
            .appendingPathComponent(info.company, isDirectory: true).appendingPathComponent(info.product, isDirectory: true)
    }

    enum Refusal: Error, Equatable, LocalizedError {
        /// The bottle path leads outside VNDegen's Bottles folder, or that folder is inside CrossOver's own.
        case outsideOwnFolder(String)

        var errorDescription: String? {
            switch self {
            case let .outsideOwnFolder(path): "VNDegen won't use the CrossOver bottle at \(path), because it isn't the one VNDegen made."
            }
        }
    }

    struct Command: Equatable, Sendable {
        var executable: URL
        var arguments: [String]
        var environment: [String: String]
    }

    enum Tool: String { case wine, cxbottle }

    /// `tool` with the bottle's own options, then `arguments`. `environment` is the one to start from, normally the
    /// app's, without anything that could name another bottle. Every `wine` call but `update` passes `--no-update`, so
    /// CrossOver never updates the bottle, which can link its folders to the Mac's again, after `harden` has run.
    func command(_ tool: Tool, _ arguments: [String], environment: [String: String]) throws -> Command {
        try checkOwnBottle()
        var environment = environment
        for name in ["CX_BOTTLE", "WINEPREFIX", "CX_MANAGED_BOTTLE_PATH", "WINESERVER", "WINELOADER"] { environment[name] = nil }
        environment["CX_BOTTLE_PATH"] = folder.path
        let noUpdate = tool == .wine && arguments.first != "--update-only" ? ["--no-update"] : []
        return Command(executable: tools.appendingPathComponent(tool.rawValue),
                       arguments: ["--bottle", path.path, "--scope", "private"] + noUpdate + arguments, environment: environment)
    }

    /// Fails closed: the bottle must be `<folder>/VNDegen` after every link is followed, the folder must not be inside
    /// CrossOver's own Application Support, the bottle itself must not be a link, and neither may the parts of it Wine
    /// and VNDegen write: C:, the registry files and the drive folder.
    func checkOwnBottle() throws {
        let home = FileManager.default.homeDirectoryForCurrentUser
        let crossOver = Self.resolved(home.appendingPathComponent("Library/Application Support/CrossOver", isDirectory: true))
        let ownFolder = Self.resolved(folder)
        let isLink = (try? FileManager.default.destinationOfSymbolicLink(atPath: path.path)) != nil
        guard !isLink, Self.resolved(path) == ownFolder + "/" + Self.name,
              ownFolder != crossOver, !ownFolder.hasPrefix(crossOver + "/")
        else { throw Refusal.outsideOwnFolder(path.path) }
        for part in ["drive_c", "dosdevices", "system.reg", "user.reg", "userdef.reg", "cxbottle.conf"] {
            try checkInside(path.appendingPathComponent(part))
        }
    }

    /// Throws unless `url`, with every link followed, is inside the bottle.
    func checkInside(_ url: URL) throws {
        guard Self.resolved(url).hasPrefix(Self.resolved(path) + "/") else { throw Refusal.outsideOwnFolder(url.path) }
    }

    /// The path with every link followed, as far as it exists, then the parts that don't exist yet. POSIX `realpath`,
    /// unlike Foundation's resolving, keeps `/private` whether or not the path exists, so two paths compare reliably.
    static func resolved(_ url: URL) -> String {
        var existing = url.standardizedFileURL
        var missing: [String] = []
        while existing.path != "/" {
            if let real = realpath(existing.path, nil) {
                defer { free(real) }
                return ([String(cString: real)] + missing).joined(separator: "/").replacingOccurrences(of: "//", with: "/")
            }
            missing.insert(existing.lastPathComponent, at: 0)
            existing.deleteLastPathComponent()
        }
        return "/" + missing.joined(separator: "/")
    }

    /// Brings the bottle up to the installed CrossOver's version, which a launch would otherwise do on its own after
    /// `harden`.
    func update(environment: [String: String]) throws -> Command {
        try command(.wine, ["--update-only"], environment: environment)
    }

    func create(environment: [String: String]) throws -> Command {
        try command(.cxbottle, ["--create", "--template", Self.template, "--description", "VNDegen's bottle for Unity games"],
                    environment: environment)
    }

    /// Runs `exe` from `workdir`, waiting until it exits, as `wine` does by default. The game goes as the first plain
    /// argument, which `wine` turns into a Windows path on Z:. `--cx-app` would look for it inside the bottle's C: drive.
    /// Wine's menu builder is off, so a game can't add launchers to the Mac's Applications folder. The `wine` script drops
    /// `WINEDLLOVERRIDES` from the environment, so `--dll` is the only way to set it.
    func run(_ exe: URL, workdir: URL, arguments: [String] = [], environment: [String: String]) throws -> Command {
        try command(.wine, ["--dll", "winemenubuilder.exe=d", "--workdir", workdir.path, exe.path] + arguments,
                    environment: environment)
    }

    /// Asks the game to close as its window's close button does. Wine's `taskkill` without `/f` posts `WM_CLOSE`, so
    /// Unity quits normally and writes its PlayerPrefs. With `force`, it ends the game's process. Either reaches only
    /// processes of that exe, never another game running in the bottle.
    func close(_ exeName: String, force: Bool = false, environment: [String: String]) throws -> Command {
        try builtin(["taskkill"] + (force ? ["/f"] : []) + ["/im", exeName], environment: environment)
    }

    /// One of Wine's own programs, such as regedit, which aren't files in the bottle for `--cx-app` to find. With
    /// `--no-convert`, `wine` leaves options such as `/S` alone instead of taking them for Mac paths, so file arguments
    /// go as `windowsPath` gives them.
    private func builtin(_ arguments: [String], environment: [String: String]) throws -> Command {
        try command(.wine, ["--no-convert"] + arguments, environment: environment)
    }

    /// A Mac path as Wine sees it on the Z: drive.
    static func windowsPath(_ url: URL) -> String {
        "Z:" + url.standardizedFileURL.path.replacingOccurrences(of: "/", with: "\\")
    }

    /// Wine's regedit, importing `file` into the bottle.
    func importRegistry(_ file: URL, environment: [String: String]) throws -> Command {
        try builtin(["regedit", "/S", Self.windowsPath(file)], environment: environment)
    }

    /// Wine's regedit, writing `key` and its subkeys to `file`.
    func exportRegistry(_ key: String, to file: URL, environment: [String: String]) throws -> Command {
        try builtin(["regedit", "/E", Self.windowsPath(file), key], environment: environment)
    }

    // MARK: Hardening

    /// The fonts copied into the bottle from the Mac's own, for games that use Unity's built-in Arial or another Windows
    /// core font. Unity reads the font file from `C:\Windows\Fonts` itself, and a new bottle has none there.
    static let coreFontFamilies = ["Arial", "Times New Roman", "Courier New", "Verdana", "Georgia", "Tahoma", "Trebuchet MS",
                                   "Comic Sans MS", "Impact"]
    static let macFonts = URL(fileURLWithPath: "/System/Library/Fonts/Supplemental", isDirectory: true)
    static let keepFile = ".vndegen-keep"

    /// Keeps the game inside the bottle: the Windows user's folders, which CrossOver links to the Mac's own Desktop,
    /// Documents, Downloads and so on, become folders of the bottle's, and every drive but C: and Z: goes. Z: stays so
    /// Wine can reach the game's folder. Each folder holds `keepFile`, because Wine replaces an empty folder with its link
    /// when it updates a bottle. Runs after `update` before every launch.
    func harden() throws {
        try checkOwnBottle()
        let fm = FileManager.default
        let bottle = Self.resolved(path)
        for base in [userFolder, driveC.appendingPathComponent("users/Public", isDirectory: true)] {
            try checkInside(base)
            for name in (try? fm.contentsOfDirectory(atPath: base.path)) ?? [] {
                let item = base.appendingPathComponent(name)
                guard let target = try? fm.destinationOfSymbolicLink(atPath: item.path) else { continue }
                let absolute = Self.resolved(URL(fileURLWithPath: target, relativeTo: base))
                guard !absolute.hasPrefix(bottle + "/") else { continue }
                try fm.removeItem(at: item)
                try fm.createDirectory(at: item, withIntermediateDirectories: false)
                try Data().write(to: item.appendingPathComponent(Self.keepFile))
            }
        }
        let devices = path.appendingPathComponent("dosdevices", isDirectory: true)
        for name in (try? fm.contentsOfDirectory(atPath: devices.path)) ?? [] where !["c:", "z:"].contains(name.lowercased()) {
            let item = devices.appendingPathComponent(name)
            // Drive letters are links. Anything else there, such as a serial port's device link, stays.
            guard name.count == 2, name.hasSuffix(":"), (try? fm.destinationOfSymbolicLink(atPath: item.path)) != nil else { continue }
            try fm.removeItem(at: item)
        }
    }

    /// Copies the core fonts the Mac has into the bottle's Fonts folder, leaving any already there.
    func installFonts(from source: URL = macFonts) throws {
        try checkOwnBottle()
        let fm = FileManager.default
        let fonts = driveC.appendingPathComponent("windows/Fonts", isDirectory: true)
        try checkInside(fonts)
        try fm.createDirectory(at: fonts, withIntermediateDirectories: true)
        try checkInside(fonts)
        let names = (try? fm.contentsOfDirectory(atPath: source.path)) ?? []
        for name in names.sorted() where Self.isCoreFont(name) {
            let target = fonts.appendingPathComponent(name)
            if fm.fileExists(atPath: target.path) { continue }
            try fm.copyItem(at: source.appendingPathComponent(name), to: target)
        }
    }

    /// `Arial.ttf` and `Arial Bold Italic.ttf`, but not `Arial Narrow.ttf` or `Arial Unicode.ttf`, which Windows doesn't
    /// have under those names.
    static func isCoreFont(_ name: String) -> Bool {
        guard name.lowercased().hasSuffix(".ttf") else { return false }
        let stem = String(name.dropLast(4))
        return coreFontFamilies.contains { family in
            stem == family || ["Bold", "Italic", "Bold Italic"].contains { stem == "\(family) \($0)" }
        }
    }
}
