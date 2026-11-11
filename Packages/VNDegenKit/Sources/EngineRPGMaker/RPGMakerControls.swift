import Foundation
import VNCore
import WebRuntime

nonisolated enum RPGMakerControls {
    static let version = 1

    static let free: [ControlInput] = [.r2, .l2, .view, .l3, .r3, .menu]
    static let gamepadButtons: [Int: ControlInput] = [6: .l2, 7: .r2, 8: .view, 9: .menu, 10: .l3, 11: .r3]
    static let reachable: Set<String> = ["ok", "cancel", "escape", "shift", "menu", "pageup", "pagedown", "up", "down", "left", "right"]
    static let defaultKeys: [String: Int] = ["tab": 9, "control": 17, "alt": 18]
    static let padConfigs: Set<String> = ["Mano_InputConfig", "Mano_GamePadConfig", "GamepadConfig", "YEP_GamepadConfig"]
    static let developerPlugins: Set<String> = [
        "GraphicalDesignMode", "AdjustPictureGraphical", "DevToolsManage", "KMS_DebugUtil", "SoR_KMSDebugUtility_MZ",
        "EventDebugger", "TRP_ParticleEditor", "TRP_SkitMZ_DevPicker", "MPI_ValueMonitor",
    ]
    static let ignored: Set<String> = ["debug", "copy"]
    static let rightClick: Set<String> = ["右クリック", "Right Click", "right_click", "light_click"]

    struct Plugin {
        var name: String
        var parameters: [String: String]
        var source: String

        var shortName: String { String(name.split(separator: "/").last ?? "") }
    }

    static func suggestion(_ folder: GameFolder) -> ControlSuggestion? {
        let plugins = enabledPlugins(folder).map { plugin in
            var plugin = plugin
            plugin.source = folder.read(servedPath: "/js/plugins/\(plugin.name).js", limit: 8 << 20)
                .map { String(decoding: $0, as: UTF8.self) } ?? ""
            return plugin
        }
        return suggestion(plugins)
    }

    static func enabledPlugins(_ folder: GameFolder) -> [Plugin] {
        guard let data = folder.read(servedPath: "/js/plugins.js", limit: 8 << 20) else { return [] }
        return enabledPlugins(String(decoding: data, as: UTF8.self))
    }

    static func enabledPlugins(_ text: String) -> [Plugin] {
        guard let start = text.firstIndex(of: "["), let end = text.lastIndex(of: "]"), start < end,
              let list = try? JSONSerialization.jsonObject(with: Data(text[start...end].utf8)) as? [[String: Any]]
        else { return [] }
        return list.compactMap { entry in
            guard entry["status"] as? Bool == true, let name = entry["name"] as? String else { return nil }
            let parameters = (entry["parameters"] as? [String: Any] ?? [:]).compactMapValues { $0 as? String }
            return Plugin(name: name, parameters: parameters, source: "")
        }
    }

    static func suggestion(_ plugins: [Plugin]) -> ControlSuggestion? {
        guard !plugins.contains(where: { padConfigs.contains($0.shortName) }) else { return nil }
        var free = free
        var padNames: Set<String> = []
        var keyNames = defaultKeys
        var mapped: [(code: Int, name: String)] = []
        for plugin in plugins where !isDeveloper(plugin) {
            for (index, name) in matches(#"gamepadMapper\[\s*(\d+)\s*\]\s*=\s*['"]([\w-]+)['"]"#, in: plugin.source) {
                if let input = gamepadButtons[index] { free.removeAll { $0 == input } }
                padNames.insert(name)
            }
            for (code, name) in matches(#"keyMapper\[\s*(\d+)\s*\]\s*=\s*['"]([\w-]+)['"]"#, in: plugin.source) {
                mapped.append((code, name))
                if keyNames[name] == nil { keyNames[name] = code }
            }
        }
        let isReachable = { (name: String) in reachable.contains(name) || padNames.contains(name) }
        func key(named name: String) -> ControlAction? {
            if name.count == 1, let letter = name.uppercased().unicodeScalars.first, (65...90).contains(letter.value) {
                return KeyboardKey(windowsKeyCode: Int(letter.value)).map(ControlAction.key)
            }
            guard !isReachable(name), let code = keyNames[name] else { return nil }
            return KeyboardKey(windowsKeyCode: code).map(ControlAction.key)
        }

        var bindings: [ControlSuggestion.Binding] = []
        for plugin in plugins {
            let parameters = plugin.parameters
            switch plugin.shortName {
            case "MessageSkip":
                if let name = parameters["SkipKey"] ?? parameters["スキップキー"], let action = key(named: name.lowercased()) {
                    bindings.append(.init(action, label: "Skip", on: [.r2]))
                }
                if let name = parameters["AutoKey"] ?? parameters["オートキー"], let action = key(named: name.lowercased()) {
                    bindings.append(.init(action, label: "Auto", on: [.l2]))
                }
            case "UTA_MessageSkip":
                if let name = parameters["Skip Key"], let action = key(named: name) {
                    bindings.append(.init(action, label: "Skip", on: [.r2]))
                }
            case let name where name.hasPrefix("MessageWindowHidden"):
                let triggers = parameters["triggerButton"].flatMap { try? JSONDecoder().decode([String].self, from: Data($0.utf8)) } ?? []
                guard !triggers.contains(where: isReachable) else { break }
                let action = triggers.contains(where: rightClick.contains) ? .click(.secondary) : triggers.lazy.compactMap(key).first
                if let action { bindings.append(.init(action, label: "Hide window", on: [.view])) }
            case "TMCommonEventKey":
                for (name, value) in parameters.sorted(by: { $0.key < $1.key }) where name.hasPrefix("commonKey") && (Int(value) ?? 0) > 0 {
                    let label = String(name.dropFirst("commonKey".count))
                    let code = label.count == 1 ? Int(label.unicodeScalars.first!.value) : label.hasPrefix("F") ? Int(label.dropFirst()).map { 111 + $0 } : nil
                    if let code, label != "F9", let key = KeyboardKey(windowsKeyCode: code) { bindings.append(.init(.key(key))) }
                }
            default:
                break
            }
        }
        for (code, name) in mapped where !isReachable(name) && !ignored.contains(name) {
            if let key = KeyboardKey(windowsKeyCode: code) { bindings.append(.init(.key(key))) }
        }
        return ControlSuggestion(placing: bindings, on: free)
    }

    static func isDeveloper(_ plugin: Plugin) -> Bool {
        developerPlugins.contains(plugin.shortName) || plugin.shortName.range(of: "debug|devtool|editor", options: [.regularExpression, .caseInsensitive]) != nil
    }

    private static func matches(_ pattern: String, in source: String) -> [(Int, String)] {
        guard !source.isEmpty, let regex = try? NSRegularExpression(pattern: pattern) else { return [] }
        let text = source as NSString
        return regex.matches(in: source, range: NSRange(location: 0, length: text.length)).compactMap { match in
            Int(text.substring(with: match.range(at: 1))).map { ($0, text.substring(with: match.range(at: 2))) }
        }
    }
}
