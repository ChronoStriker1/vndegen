// Boots every Ren'Py game in a folder the way VNDegen does, with the probe in zz_vndegen.rpy, and prints a Markdown
// table: the runtime each game got and why, whether it reached the main menu, and whether its folder stayed unchanged.
//   swift run RenPyCompat <games folder> --runtimes <folder with 7/ and 8/> [--runtime 7|8] [--timeout 45] [--out <folder>]
// scripts/renpy-compat.sh fetches the runtimes, builds this, and runs it.
#if os(macOS)
import EngineRenPy
import Foundation
import VNCore

struct Options {
    var games: URL
    var runtimes: URL
    var runtime: RenPyRuntime?
    var timeout = 45.0
    var out = URL(fileURLWithPath: NSTemporaryDirectory()).appendingPathComponent("renpy-compat", isDirectory: true)

    init(_ arguments: [String]) {
        var positional: [String] = []
        var runtimes: String?
        var rest = arguments.makeIterator()
        while let argument = rest.next() {
            switch argument {
            case "--runtimes": runtimes = rest.next()
            case "--runtime": runtime = rest.next().flatMap(RenPyRuntime.init(rawValue:))
            case "--timeout": timeout = rest.next().flatMap(Double.init) ?? timeout
            case "--out": out = URL(fileURLWithPath: rest.next() ?? out.path, isDirectory: true)
            default: positional.append(argument)
            }
        }
        guard let games = positional.first, let runtimes else {
            FileHandle.standardError.write(Data("usage: RenPyCompat <games folder> --runtimes <folder> [--runtime 7|8] [--timeout 45] [--out <folder>]\n".utf8))
            exit(2)
        }
        self.games = URL(fileURLWithPath: games, isDirectory: true)
        self.runtimes = URL(fileURLWithPath: runtimes, isDirectory: true)
    }
}

/// Every file under `folder` with its size and modification date, to show that a run wrote nothing there.
func snapshot(_ folder: URL) -> [String: String] {
    var files: [String: String] = [:]
    let keys: [URLResourceKey] = [.fileSizeKey, .contentModificationDateKey, .isRegularFileKey]
    let walker = FileManager.default.enumerator(at: folder, includingPropertiesForKeys: keys)
    while let url = walker?.nextObject() as? URL {
        guard let values = try? url.resourceValues(forKeys: Set(keys)), values.isRegularFile == true else { continue }
        let stamp = values.contentModificationDate?.timeIntervalSince1970 ?? 0
        files[String(url.path.dropFirst(folder.path.count))] = "\(values.fileSize ?? 0) \(stamp)"
    }
    return files
}

/// The first line that names a Python exception in Ren'Py's traceback, or else in its log.
func firstError(_ files: [URL]) -> String? {
    for file in files {
        guard let text = try? String(contentsOf: file, encoding: .utf8) else { continue }
        if let line = text.split(separator: "\n").first(where: { $0.firstMatch(of: #/^[A-Za-z.]*(Error|Exception)\b/#) != nil }) {
            return String(line.prefix(120))
        }
    }
    return nil
}

let options = Options(Array(CommandLine.arguments.dropFirst()))
guard let support = RenPyCommand.support else { fatalError("EngineRenPy's Support folder is missing") }
let names = ((try? FileManager.default.contentsOfDirectory(atPath: options.games.path)) ?? []).filter { !$0.hasPrefix(".") }.sorted()

print("| Game | Version | Runtime | Result | Seconds | Game folder | Error |")
print("|---|---|---|---|---|---|---|")
for name in names {
    let folder = options.games.appendingPathComponent(name, isDirectory: true)
    guard let layout = RenPyLayout(FolderProbe(scanning: folder)) else { continue }
    let runtime = options.runtime ?? layout.runtime
    let out = options.out.appendingPathComponent("\(name)/\(runtime.rawValue)", isDirectory: true)
    let data = out.appendingPathComponent("data", isDirectory: true)
    let probe = out.appendingPathComponent("probe", isDirectory: true)
    try? FileManager.default.removeItem(at: out)
    try FileManager.default.createDirectory(at: probe, withIntermediateDirectories: true)
    try FileManager.default.createDirectory(at: data, withIntermediateDirectories: true)
    let stdout = out.appendingPathComponent("stdout.txt")
    FileManager.default.createFile(atPath: stdout.path, contents: nil)

    let base = folder.appendingPathComponent(layout.path(""), isDirectory: true)
    var command = RenPyCommand(base: base, runtime: runtime, runtimes: options.runtimes, support: support, dataDirectory: data,
                               placement: .frame(CGRect(x: 80, y: 80, width: 1280, height: 720)))
    command.environment["VNDEGEN_PROBE"] = probe.path
    command.environment["VNDEGEN_PROBE_TIMEOUT"] = String(options.timeout)

    let before = snapshot(folder)
    let process = Process()
    process.executableURL = command.executable
    process.arguments = command.arguments
    process.environment = command.environment
    process.standardOutput = try FileHandle(forWritingTo: stdout)
    process.standardError = process.standardOutput
    let start = Date.now
    try process.run()
    let pid = process.processIdentifier
    DispatchQueue.global().asyncAfter(deadline: .now() + options.timeout + 30) { kill(pid, SIGKILL) }
    process.waitUntilExit()
    let seconds = Int(Date.now.timeIntervalSince(start))

    let changed = snapshot(folder).filter { before[$0.key] != $0.value }.count
        + before.keys.filter { !FileManager.default.fileExists(atPath: folder.path + $0) }.count
    let result = (try? String(contentsOf: probe.appendingPathComponent("result.txt"), encoding: .utf8))?
        .trimmingCharacters(in: .whitespacesAndNewlines) ?? "no result, exit \(process.terminationStatus)"
    let error = firstError([data.appendingPathComponent("logs/traceback.txt"), stdout]) ?? ""
    let version = layout.version ?? "?"
    print("| \(name) | \(version) | \(runtime.displayName) (\(options.runtime == nil ? layout.evidence.rawValue : "forced")) | \(result) | \(seconds) | \(changed == 0 ? "unchanged" : "\(changed) files changed") | \(error) |")
}
#else
print("RenPyCompat runs on macOS only.")
#endif
