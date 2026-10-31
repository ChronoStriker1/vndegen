import SwiftUI
import VNCore
#if canImport(UIKit)
import UIKit
#endif

/// What Settings' Plugins page says about an engine plugin beyond its descriptor: the engine versions it covers, what
/// runs the games, and under which license.
struct PluginInfo {
    let versions: String
    let runtime: String
    let license: String
    /// Why it doesn't run on this device, when it doesn't.
    var notHere: String?

    /// Keyed by engine ID. A new engine adds its entry here; one without an entry still lists its name.
    static let catalog: [EngineID: PluginInfo] = {
        let rgss = "VNDegen's RGSS runtime, on MRI Ruby with no JIT"
        let rgssLicense = "VNDegen, MIT. Ruby, Ruby license or BSD 2-clause"
        let web = "The game's own scripts in WebKit, with VNDegen's NW.js layer"
        #if os(macOS)
        let renPy = "Ren'Py 8.5.3 and 7.8.7, each game in its own process"
        #else
        let renPy = "Ren'Py 8.5.3, built into the app"
        #endif
        return [
            EngineID("rpgmaker.mz"): PluginInfo(versions: "RPG Maker MZ", runtime: web, license: "VNDegen, MIT"),
            EngineID("rpgmaker.mv"): PluginInfo(versions: "RPG Maker MV", runtime: web, license: "VNDegen, MIT"),
            EngineID("tyranoscript"): PluginInfo(versions: "TyranoScript and TyranoBuilder, including NW.js and Electron builds",
                                                 runtime: web, license: "VNDegen, MIT"),
            EngineID("renpy"): PluginInfo(versions: "Ren'Py 6.99 to 8", runtime: renPy, license: "Ren'Py, MIT, with LGPL libraries"),
            EngineID("rpgmaker.xp"): PluginInfo(versions: "RPG Maker XP (RGSS1)", runtime: rgss, license: rgssLicense),
            EngineID("rpgmaker.vx"): PluginInfo(versions: "RPG Maker VX (RGSS2)", runtime: rgss, license: rgssLicense),
            EngineID("rpgmaker.vxace"): PluginInfo(versions: "RPG Maker VX Ace (RGSS3)", runtime: rgss, license: rgssLicense),
            EngineID("kirikiri"): PluginInfo(versions: "KiriKiri 2 and KiriKiri Z", runtime: "krkrsdl3, an SDL3 port of KiriKiri Z",
                                             license: "KiriKiri Z license, BSD 3-clause"),
            EngineID("artemis"): PluginInfo(versions: "Artemis", runtime: "art3m1s-core, with Lua 5.1 and no JIT", license: "MPL 2.0"),
            EngineID("godot"): PluginInfo(versions: "Godot 3 and Godot 4, from Godot 3.3 to 4.7 exports",
                                          runtime: "Godot 4.7 and Godot 3.6, built into the app", license: "MIT",
                                          notHere: "Godot is built for Apple silicon only, so an Intel Mac lists these games but can't run them."),
            EngineID("flash"): PluginInfo(versions: "Flash, ActionScript 1, 2, and 3", runtime: "Ruffle, in WebKit with WebGL",
                                          license: "Ruffle, MIT or Apache 2.0"),
            EngineID("nscripter"): PluginInfo(versions: "NScripter, with NSLua", runtime: "VNDegen's NScripter runtime, in C++",
                                              license: "VNDegen, MIT"),
            EngineID("wolfrpg"): PluginInfo(versions: "Wolf RPG Editor 2 and 3", runtime: "VNDegen's Wolf RPG Editor runtime, in C++",
                                            license: "VNDegen, MIT"),
            EngineID("alicesoft"): PluginInfo(versions: "AliceSoft System 3.0 to 3.9", runtime: "VNDegen's System 3 runtime, in C",
                                              license: "VNDegen, MIT"),
        ]
    }()
}

/// Settings' Plugins page: every engine plugin built into the app, what it covers, and how many library games use it.
struct PluginsView: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav

    var body: some View {
        ScrollView {
            LazyVStack(alignment: .leading, spacing: 12) {
                Text("Engine plugins are built into VNDegen, so they can't be added or removed. Each recognizes its own games' files and runs them.")
                    .font(.system(size: 13))
                    .foregroundStyle(Theme.secondaryText)
                    .fixedSize(horizontal: false, vertical: true)
                ForEach(hub.library.engines.map(\.descriptor), id: \.id) { descriptor in
                    row(descriptor)
                }
            }
            .padding(.horizontal, 22)
            .padding(.bottom, 22)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .scrollPager(nav.pager)
    }

    private func row(_ descriptor: EngineDescriptor) -> some View {
        let info = PluginInfo.catalog[descriptor.id]
        let runsHere = descriptor.platforms.contains(.current)
        let count = hub.library.games.filter { $0.engine == descriptor.id }.count
        return VStack(alignment: .leading, spacing: 8) {
            HStack(alignment: .firstTextBaseline) {
                Text(descriptor.displayName).font(.system(size: 17, weight: .bold))
                Spacer()
                Text(count == 1 ? "1 game" : "\(count) games")
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(Theme.secondaryText)
            }
            if let info {
                fact("Covers", info.versions)
                fact("Runs on", info.runtime)
                fact("License", info.license)
            }
            Label(runsHere ? "Runs on this \(Self.device)" : (info?.notHere ?? "Doesn't run on this \(Self.device) yet."),
                  systemImage: runsHere ? "checkmark.circle.fill" : "xmark.circle.fill")
                .font(.system(size: 13, weight: .semibold))
                .foregroundStyle(runsHere ? Theme.secondaryText : Theme.accent)
                .fixedSize(horizontal: false, vertical: true)
        }
        .padding(16)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(Theme.surface, in: .rect(cornerRadius: 14, style: .continuous))
        .overlay { RoundedRectangle(cornerRadius: 14, style: .continuous).strokeBorder(Theme.stroke) }
        .accessibilityElement(children: .combine)
    }

    private func fact(_ label: String, _ value: String) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 10) {
            Text(label).foregroundStyle(Theme.tertiaryText).frame(width: 64, alignment: .leading)
            Text(value).foregroundStyle(.white.opacity(0.86)).fixedSize(horizontal: false, vertical: true)
        }
        .font(.system(size: 13))
    }

    private static var device: String {
        #if os(macOS)
        "Mac"
        #else
        UIDevice.current.localizedModel
        #endif
    }
}
