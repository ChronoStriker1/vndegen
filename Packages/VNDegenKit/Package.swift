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
        .library(name: "EngineRenPy", targets: ["EngineRenPy"]),
        .library(name: "EngineKiriKiri", targets: ["EngineKiriKiri"]),
        .library(name: "EngineRGSS", targets: ["EngineRGSS"]),
        .library(name: "EngineNScripter", targets: ["EngineNScripter"]),
        .library(name: "FrameHost", targets: ["FrameHost"]),
        .library(name: "Metadata", targets: ["Metadata"]),
        .library(name: "GameInput", targets: ["GameInput"]),
        .library(name: "Lock", targets: ["Lock"]),
        .library(name: "HubUI", targets: ["HubUI"]),
        .library(name: "SaveSync", targets: ["SaveSync"]),
        .library(name: "Rclone", targets: ["Rclone"]),
        .library(name: "ArchiveImport", targets: ["ArchiveImport"]),
        .library(name: "RuntimePackages", targets: ["RuntimePackages"]),
    ],
    targets: [
        .target(name: "VNCore"),
        // Controller focus rows, the on-screen keyboard a controller types on, and controller scrolling, for every
        // module that draws a screen.
        .target(name: "ControllerUI", dependencies: ["VNCore"], swiftSettings: mainActorByDefault),
        .target(name: "GameInput", dependencies: ["VNCore"], swiftSettings: mainActorByDefault),
        .target(name: "WebRuntime", dependencies: ["VNCore"], exclude: ["vorbis.wasm.LICENSE"], resources: [.process("Resources")],
                swiftSettings: mainActorByDefault),
        .target(name: "EngineRPGMaker", dependencies: ["VNCore", "WebRuntime"], resources: [.process("Resources")], swiftSettings: mainActorByDefault),
        .target(name: "EngineTyrano", dependencies: ["VNCore", "WebRuntime"], resources: [.process("Resources")], swiftSettings: mainActorByDefault),
        // Support holds the Python launcher and the Ren'Py hook script that Ren'Py loads from disk.
        .target(name: "EngineRenPy", dependencies: ["VNCore", "CRenPy"], resources: [.copy("Support")], swiftSettings: mainActorByDefault),
        // Runs Ren'Py inside the app on iOS. Ren'Py's static libraries are linked by the app target, not here.
        .target(name: "CRenPy"),
        // The KiriKiri engine's C interface. The engine itself is a framework the app embeds, found at run time.
        .target(name: "CKiriKiri"),
        .target(name: "EngineKiriKiri", dependencies: ["VNCore", "CKiriKiri"], swiftSettings: mainActorByDefault),
        // The RGSS engine's C interface. The engine itself is a framework the app embeds, opened at run time.
        .target(name: "CRGSS"),
        .target(name: "EngineRGSS", dependencies: ["VNCore", "CRGSS"], resources: [.process("Resources")], swiftSettings: mainActorByDefault),
        // VNDegen's own NScripter runtime, in C++, with vnns.h as its C interface. third_party holds stb_image,
        // stb_vorbis and pl_mpeg, which the runtime's sources include directly rather than compile on their own.
        .target(
            name: "CNScripter", exclude: ["third_party", "core/sjis_table.inc"],
            linkerSettings: [.linkedFramework("CoreText", .when(platforms: [.iOS, .macOS])),
                             .linkedFramework("CoreGraphics", .when(platforms: [.iOS, .macOS]))]),
        // Presents a frame a runtime drew on the CPU through Metal, and plays the sound it mixes, for the runtimes that
        // composite in software.
        .target(name: "FrameHost", dependencies: ["VNCore"], swiftSettings: mainActorByDefault),
        .target(name: "EngineNScripter", dependencies: ["VNCore", "CNScripter", "FrameHost"], swiftSettings: mainActorByDefault),
        .executableTarget(name: "RenPyCompat", dependencies: ["EngineRenPy", "VNCore"]),
        .target(name: "Metadata", dependencies: ["VNCore", "ControllerUI"]),
        .target(name: "Lock", dependencies: ["VNCore", "ControllerUI"], swiftSettings: mainActorByDefault),
        .target(name: "HubUI", dependencies: ["VNCore", "ControllerUI", "GameInput", "Metadata", "Lock", "SaveSync", "ArchiveImport", "RuntimePackages"], swiftSettings: mainActorByDefault),
        .target(name: "SaveSync", dependencies: ["VNCore", "ControllerUI", "Rclone"]),
        // rclone's library, built by scripts/rclone-runtime.sh from Rclone/rclone.go. Not in git: scripts/build.sh makes it.
        .binaryTarget(name: "CRclone", path: "build/CRclone.xcframework"),
        .target(name: "Rclone", dependencies: ["CRclone"],
                linkerSettings: [.linkedFramework("CoreFoundation"), .linkedFramework("Security"), .linkedLibrary("resolv")]),
        // The system libarchive plus zlib's infback9 for Deflate64. Lives inside ArchiveImport's folder.
        .target(
            name: "CArchive", path: "Sources/ArchiveImport/CArchive", exclude: ["LICENSE-zlib.txt"],
            linkerSettings: [.linkedLibrary("archive"), .linkedLibrary("z")]),
        .target(name: "ArchiveImport", dependencies: ["CArchive", "ControllerUI"], exclude: ["CArchive", "README.md"], swiftSettings: mainActorByDefault),
        // Downloads, checks, and unpacks runtime packages such as the RPG Maker RTPs. CLzmaDec is the LZMA SDK's decoder,
        // which Inno Setup installers need.
        .target(name: "CLzmaDec", path: "Sources/RuntimePackages/CLzmaDec"),
        .target(name: "RuntimePackages", dependencies: ["VNCore", "CLzmaDec"], exclude: ["CLzmaDec", "LICENSES.md", "LICENSE-innoextract.txt", "LICENSE-LZMA-SDK.txt"]),
        .testTarget(name: "VNCoreTests", dependencies: ["VNCore"]),
        .testTarget(name: "WebRuntimeTests", dependencies: ["WebRuntime"], exclude: ["Fixtures"]),
        .testTarget(name: "EngineRPGMakerTests", dependencies: ["EngineRPGMaker", "WebRuntime"], exclude: ["Fixtures"]),
        .testTarget(name: "EngineTyranoTests", dependencies: ["EngineTyrano", "WebRuntime"]),
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
        .testTarget(name: "EngineNScripterTests", dependencies: ["EngineNScripter", "CNScripter"]),
    ],
    cxxLanguageStandard: .cxx17
)
