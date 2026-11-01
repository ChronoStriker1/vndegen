// swift-tools-version: 6.2
import PackageDescription

let mainActorByDefault: [SwiftSetting] = [.defaultIsolation(MainActor.self)]

let package = Package(
    name: "VNDegenKit",
    platforms: [.iOS(.v18), .macOS(.v15)],
    products: [
        .library(name: "VNCore", targets: ["VNCore"]),
        .library(name: "WebRuntime", targets: ["WebRuntime"]),
        .library(name: "EngineRPGMaker", targets: ["EngineRPGMaker"]),
        .library(name: "EngineTyrano", targets: ["EngineTyrano"]),
        .library(name: "EngineFlash", targets: ["EngineFlash"]),
        .library(name: "EngineRenPy", targets: ["EngineRenPy"]),
        .library(name: "EngineKiriKiri", targets: ["EngineKiriKiri"]),
        .library(name: "EngineRGSS", targets: ["EngineRGSS"]),
        .library(name: "EngineArtemis", targets: ["EngineArtemis"]),
        .library(name: "EngineGodot", targets: ["EngineGodot"]),
        .library(name: "EngineNScripter", targets: ["EngineNScripter"]),
        .library(name: "FrameHost", targets: ["FrameHost"]),
        .library(name: "EngineWolfRPG", targets: ["EngineWolfRPG"]),
        .library(name: "EngineAliceSoft", targets: ["EngineAliceSoft"]),
        .library(name: "Metadata", targets: ["Metadata"]),
        .library(name: "GameInput", targets: ["GameInput"]),
        .library(name: "Lock", targets: ["Lock"]),
        .library(name: "HubUI", targets: ["HubUI"]),
        .library(name: "SaveSync", targets: ["SaveSync"]),
        .library(name: "Rclone", targets: ["Rclone"]),
        .library(name: "ArchiveImport", targets: ["ArchiveImport"]),
        .library(name: "RuntimePackages", targets: ["RuntimePackages"]),
        .library(name: "FontPack", targets: ["FontPack"]),
    ],
    targets: [
        .target(name: "VNCore"),
        // Controller focus rows, the on-screen keyboard a controller types on, and controller scrolling, for every
        // module that draws a screen.
        .target(name: "ControllerUI", dependencies: ["VNCore"], swiftSettings: mainActorByDefault),
        .target(name: "GameInput", dependencies: ["VNCore"], swiftSettings: mainActorByDefault),
        // Windows font substitutes. CVNFont's files are links to Engines/Shared/vnfont.[ch], which the native engines
        // compile too.
        .target(name: "CVNFont"),
        .target(name: "FontPack", dependencies: ["CVNFont", "VNCore"]),
        // General MIDI music for the native engines. CVNMidi's files are links to Engines/Shared/vnmidi.[ch] and the
        // libraries vnmidi.c compiles into itself. FontPack's MIDISoundFont finds the SoundFont the app bundles.
        .target(name: "CVNMidi", exclude: ["third_party"]),
        // Movies for the native engines: MPEG-1 and Ogg Theora with Opus. CVNVideo's files are links to
        // Engines/Shared/vnvideo.[ch] and the libraries in Engines/Shared/third_party it builds. Decoding runs on the CPU,
        // so Debug builds are optimized too. -w keeps the unmodified libraries' warnings out; scripts/build.sh test
        // builds vnvideo.c itself with -Werror.
        .target(
            name: "CVNVideo",
            sources: ["vnvideo.c", "third_party/ogg/src", "third_party/theora/lib", "third_party/opus/celt",
                      "third_party/opus/silk", "third_party/opus/src"],
            cSettings: [.define("HAVE_CONFIG_H"), .headerSearchPath("third_party/ogg/include"),
                        .headerSearchPath("third_party/theora/include"), .headerSearchPath("third_party/opus"),
                        .headerSearchPath("third_party/opus/include"), .headerSearchPath("third_party/opus/celt"),
                        .headerSearchPath("third_party/opus/silk"), .unsafeFlags(["-O2", "-w"])]),
        .target(name: "WebRuntime", dependencies: ["VNCore", "FontPack"], exclude: ["vorbis.wasm.LICENSE"], resources: [.process("Resources")],
                swiftSettings: mainActorByDefault),
        .target(name: "EngineRPGMaker", dependencies: ["VNCore", "WebRuntime", "FontPack"], resources: [.process("Resources")], swiftSettings: mainActorByDefault),
        .target(name: "EngineTyrano", dependencies: ["VNCore", "WebRuntime", "FontPack"], resources: [.process("Resources")], swiftSettings: mainActorByDefault),
        .target(name: "EngineFlash", dependencies: ["VNCore", "WebRuntime", "FontPack"], resources: [.process("Resources")], swiftSettings: mainActorByDefault),
        // Support holds the Python launcher and the Ren'Py hook script that Ren'Py loads from disk.
        .target(name: "EngineRenPy", dependencies: ["VNCore", "CRenPy"], resources: [.copy("Support")], swiftSettings: mainActorByDefault),
        // Runs Ren'Py inside the app on iOS. Ren'Py's static libraries are linked by the app target, not here.
        .target(name: "CRenPy"),
        // The KiriKiri engine's C interface. The engine itself is a framework the app embeds, found at run time.
        .target(name: "CKiriKiri"),
        .target(name: "EngineKiriKiri", dependencies: ["VNCore", "CKiriKiri", "FontPack"], swiftSettings: mainActorByDefault),
        // VNDegen's own RGSS runtime for RPG Maker XP, VX and VX Ace, in C, with vnrgss.h as its C interface. It runs
        // games' scripts on MRI Ruby, which scripts/rgss-ruby.sh builds into build/RGSSRuby.xcframework (not in git;
        // scripts/build.sh makes it). third_party holds stb and dr_libs, which the runtime's sources include directly.
        // Engines/RGSSRuntime/host builds the same sources for the Linux test tools.
        .binaryTarget(name: "RGSSRuby", path: "build/RGSSRuby.xcframework"),
        .target(
            name: "CRGSS", dependencies: ["RGSSRuby", "CVNFont", "CVNMidi", "CVNVideo"], exclude: ["third_party"],
            // The runtime draws every frame on the CPU, so Debug builds are optimized too, as NScripter's are.
            cSettings: [.unsafeFlags(["-O2"])],
            linkerSettings: [.linkedLibrary("z")]),
        .target(name: "EngineRGSS", dependencies: ["VNCore", "CRGSS", "FrameHost", "FontPack"], resources: [.process("Resources")], swiftSettings: mainActorByDefault),
        // The Godot engines' C interface. The engines are frameworks the app embeds, opened at run time.
        .target(name: "CGodot"),
        .target(name: "EngineGodot", dependencies: ["VNCore", "CGodot", "FontPack"], swiftSettings: mainActorByDefault),
        // VNDegen's own NScripter runtime, in C++, with vnns.h as its C interface. third_party holds stb_image,
        // stb_vorbis and pl_mpeg, which the runtime's sources include directly rather than compile on their own.
        .target(
            name: "CNScripter", dependencies: ["CVNFont"], exclude: ["third_party", "core/sjis_table.inc"],
            // The runtime draws every frame on the CPU. Unoptimized, a Debug build misses frames that a Release build
            // makes, and Debug builds are what the simulators and the Mac app are tested with.
            cSettings: [.unsafeFlags(["-O2"])], cxxSettings: [.unsafeFlags(["-O2"])],
            // The system's bzip2 unpacks the NBZ sound files one game's plugin would.
            linkerSettings: [.linkedFramework("CoreText", .when(platforms: [.iOS, .macOS])),
                             .linkedFramework("CoreGraphics", .when(platforms: [.iOS, .macOS])),
                             .linkedLibrary("bz2")]),
        // Presents a frame a runtime drew on the CPU through Metal, and plays the sound it mixes, for the runtimes that
        // composite in software.
        .target(name: "FrameHost", dependencies: ["VNCore"], swiftSettings: mainActorByDefault),
        .target(name: "EngineNScripter", dependencies: ["VNCore", "CNScripter", "FrameHost"], swiftSettings: mainActorByDefault),
        // VNDegen's own Wolf RPG Editor runtime, in C++20, with vnwolf.h as its C interface. third_party holds stb,
        // dr_libs and TinySoundFont, which the runtime's sources include directly. Engines/WolfRPG/host builds the same
        // sources for the Linux test tools.
        .target(
            name: "CWolfRPG", dependencies: ["CVNFont", "CVNMidi"], exclude: ["third_party"],
            // The package builds C++17; the runtime needs C++20. It draws every frame on the CPU, so Debug builds are
            // optimized too, as NScripter's are.
            cSettings: [.unsafeFlags(["-O2"])], cxxSettings: [.unsafeFlags(["-std=c++20", "-O2"])],
            linkerSettings: [.linkedFramework("CoreText", .when(platforms: [.iOS, .macOS])),
                             .linkedFramework("CoreGraphics", .when(platforms: [.iOS, .macOS]))]),
        .target(name: "EngineWolfRPG", dependencies: ["VNCore", "CWolfRPG", "FrameHost", "FontPack"], swiftSettings: mainActorByDefault),
        // AliceSoft System 4's runtime, VNDegen's own clean-room work (docs/system4-cleanroom.md). C++20 with a C
        // interface, include/vns4.h. It draws every frame on the CPU, so Debug builds are optimized too.
        .target(
            name: "CSystem4", dependencies: ["CVNFont"], exclude: ["third_party"],
            cSettings: [.unsafeFlags(["-O2"])], cxxSettings: [.unsafeFlags(["-std=c++20", "-O2"])],
            linkerSettings: [.linkedLibrary("z"),
                             .linkedFramework("ImageIO", .when(platforms: [.iOS, .macOS])),
                             .linkedFramework("CoreText", .when(platforms: [.iOS, .macOS])),
                             .linkedFramework("CoreGraphics", .when(platforms: [.iOS, .macOS]))]),
        // AliceSoft System 3.x and System 4 games, each on VNDegen's own clean-room runtime.
        // VNDegen's System 3 runtime, in C, with the shared font and MIDI code. Engines/System3/host builds the same
        // sources for the Linux test tools. It draws every frame on the CPU, so Debug builds are optimized too.
        .target(name: "CSystem3", dependencies: ["CVNFont", "CVNMidi"], exclude: ["third_party"], cSettings: [.unsafeFlags(["-O2"])]),
        .target(name: "EngineAliceSoft", dependencies: ["VNCore", "CSystem3", "CSystem4", "FrameHost", "FontPack"], swiftSettings: mainActorByDefault),
        // The Artemis engine's C interface and the Ogg Vorbis decoder its audio uses. The engine itself is a static
        // library the app links, found at run time.
        .target(name: "CArtemis", exclude: ["third_party"]),
        .target(name: "EngineArtemis", dependencies: ["VNCore", "CArtemis", "CVNVideo", "FontPack"], swiftSettings: mainActorByDefault),
        .executableTarget(name: "RenPyCompat", dependencies: ["EngineRenPy", "VNCore"]),
        .target(name: "Metadata", dependencies: ["VNCore", "ControllerUI"]),
        .target(name: "Lock", dependencies: ["VNCore", "ControllerUI"], swiftSettings: mainActorByDefault),
        .target(name: "HubUI", dependencies: ["VNCore", "ControllerUI", "GameInput", "Metadata", "Lock", "SaveSync", "ArchiveImport", "RuntimePackages", "FontPack"], swiftSettings: mainActorByDefault),
        .target(name: "SaveSync", dependencies: ["VNCore", "ControllerUI", "Rclone"]),
        // rclone's library, built by scripts/rclone-runtime.sh from Rclone/rclone.go. Not in git: scripts/build.sh makes it.
        .binaryTarget(name: "CRclone", path: "build/CRclone.xcframework"),
        .target(name: "Rclone", dependencies: ["CRclone"],
                linkerSettings: [.linkedFramework("CoreFoundation"), .linkedFramework("Security"), .linkedLibrary("resolv")]),
        // The system libarchive plus zlib's infback9 for Deflate64. Lives inside ArchiveImport's folder.
        .target(
            name: "CArchive", path: "Sources/ArchiveImport/CArchive", exclude: ["LICENSE-zlib.txt"],
            linkerSettings: [.linkedLibrary("archive"), .linkedLibrary("z")]),
        .target(name: "ArchiveImport", dependencies: ["CArchive", "ControllerUI", "VNCore"], exclude: ["CArchive", "README.md"], swiftSettings: mainActorByDefault),
        // Downloads, checks, and unpacks runtime packages such as the RPG Maker RTPs. CLzmaDec is the LZMA SDK's decoder,
        // which Inno Setup installers need.
        .target(name: "CLzmaDec", path: "Sources/RuntimePackages/CLzmaDec"),
        .target(name: "RuntimePackages", dependencies: ["VNCore", "CLzmaDec"], exclude: ["CLzmaDec", "LICENSES.md", "LICENSE-innoextract.txt", "LICENSE-LZMA-SDK.txt"]),
        // Tests read fixtures by file path, so SwiftPM must not treat them as resources.
        .testTarget(name: "VNCoreTests", dependencies: ["VNCore"], exclude: ["Fixtures"]),
        .testTarget(name: "WebRuntimeTests", dependencies: ["WebRuntime", "FontPack"], exclude: ["Fixtures"]),
        .testTarget(name: "EngineRPGMakerTests", dependencies: ["EngineRPGMaker", "WebRuntime"], exclude: ["Fixtures"]),
        .testTarget(name: "EngineTyranoTests", dependencies: ["EngineTyrano", "WebRuntime"]),
        .testTarget(name: "EngineFlashTests", dependencies: ["EngineFlash", "WebRuntime", "FontPack"]),
        // RenPyCompat is built for the tests, which run it as a command.
        .testTarget(name: "EngineRenPyTests", dependencies: ["EngineRenPy", "GameInput", "RenPyCompat"]),
        // Tests read fixtures by file path, so SwiftPM must not treat them as resources.
        .testTarget(name: "EngineKiriKiriTests", dependencies: ["EngineKiriKiri", "GameInput", "HubUI"], exclude: ["Fixtures"]),
        .testTarget(name: "MetadataTests", dependencies: ["Metadata"], exclude: ["Fixtures"]),
        .testTarget(name: "GameInputTests", dependencies: ["GameInput"]),
        .testTarget(name: "ControllerUITests", dependencies: ["ControllerUI", "VNCore"]),
        .testTarget(name: "LockTests", dependencies: ["Lock", "VNCore"]),
        .testTarget(name: "HubUITests", dependencies: ["HubUI", "VNCore", "GameInput", "Metadata", "Lock", "SaveSync", "EngineRenPy"]),
        .testTarget(name: "SaveSyncTests", dependencies: ["SaveSync", "Rclone", "EngineRPGMaker", "EngineRenPy"], exclude: ["Fixtures"]),
        .testTarget(name: "RcloneTests", dependencies: ["Rclone", "CRclone"]),
        .testTarget(name: "ArchiveImportTests", dependencies: ["ArchiveImport"], exclude: ["Fixtures"]),
        .testTarget(name: "RuntimePackagesTests", dependencies: ["RuntimePackages"]),
        .testTarget(name: "EngineRGSSTests", dependencies: ["EngineRGSS"]),
        .testTarget(name: "EngineArtemisTests", dependencies: ["EngineArtemis"]),
        .testTarget(name: "EngineGodotTests", dependencies: ["EngineGodot"]),
        .testTarget(name: "EngineNScripterTests", dependencies: ["EngineNScripter", "CNScripter"], exclude: ["Fixtures"]),
        .testTarget(name: "EngineWolfRPGTests", dependencies: ["EngineWolfRPG"]),
        .testTarget(name: "EngineAliceSoftTests", dependencies: ["EngineAliceSoft"]),
        .testTarget(name: "FontPackTests", dependencies: ["FontPack"]),
    ],
    cxxLanguageStandard: .cxx17
)
