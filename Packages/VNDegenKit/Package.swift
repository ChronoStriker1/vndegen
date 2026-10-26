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
        .library(name: "Metadata", targets: ["Metadata"]),
        .library(name: "GameInput", targets: ["GameInput"]),
        .library(name: "Lock", targets: ["Lock"]),
        .library(name: "HubUI", targets: ["HubUI"]),
        .library(name: "SaveSync", targets: ["SaveSync"]),
        .library(name: "ArchiveImport", targets: ["ArchiveImport"]),
    ],
    targets: [
        .target(name: "VNCore"),
        .target(name: "GameInput", dependencies: ["VNCore"], swiftSettings: mainActorByDefault),
        .target(name: "WebRuntime", dependencies: ["VNCore"], exclude: ["vorbis.wasm.LICENSE"], resources: [.process("Resources")],
                swiftSettings: mainActorByDefault),
        .target(name: "EngineRPGMaker", dependencies: ["VNCore", "WebRuntime"], resources: [.process("Resources")], swiftSettings: mainActorByDefault),
        .target(name: "EngineTyrano", dependencies: ["VNCore", "WebRuntime"], resources: [.process("Resources")], swiftSettings: mainActorByDefault),
        // Support holds the Python launcher and the Ren'Py hook script that the helper process loads from disk.
        .target(name: "EngineRenPy", dependencies: ["VNCore"], resources: [.copy("Support")], swiftSettings: mainActorByDefault),
        .executableTarget(name: "RenPyCompat", dependencies: ["EngineRenPy", "VNCore"]),
        .target(name: "Metadata", dependencies: ["VNCore"]),
        .target(name: "Lock", dependencies: ["VNCore"], swiftSettings: mainActorByDefault),
        .target(name: "HubUI", dependencies: ["VNCore", "GameInput", "Metadata", "Lock", "SaveSync", "ArchiveImport"], swiftSettings: mainActorByDefault),
        .target(name: "SaveSync", dependencies: ["VNCore"]),
        // The system libarchive plus zlib's infback9 for Deflate64. Lives inside ArchiveImport's folder.
        .target(
            name: "CArchive", path: "Sources/ArchiveImport/CArchive", exclude: ["LICENSE-zlib.txt"],
            linkerSettings: [.linkedLibrary("archive"), .linkedLibrary("z")]),
        .target(name: "ArchiveImport", dependencies: ["CArchive"], exclude: ["CArchive", "README.md"], swiftSettings: mainActorByDefault),
        .testTarget(name: "VNCoreTests", dependencies: ["VNCore"]),
        .testTarget(name: "WebRuntimeTests", dependencies: ["WebRuntime"], exclude: ["Fixtures"]),
        .testTarget(name: "EngineRPGMakerTests", dependencies: ["EngineRPGMaker", "WebRuntime"]),
        .testTarget(name: "EngineTyranoTests", dependencies: ["EngineTyrano", "WebRuntime"]),
        // RenPyCompat is built for the tests, which run it as a command.
        .testTarget(name: "EngineRenPyTests", dependencies: ["EngineRenPy", "GameInput", "RenPyCompat"]),
        // Tests read fixtures by file path, so SwiftPM must not treat them as resources.
        .testTarget(name: "MetadataTests", dependencies: ["Metadata"], exclude: ["Fixtures"]),
        .testTarget(name: "GameInputTests", dependencies: ["GameInput"]),
        .testTarget(name: "LockTests", dependencies: ["Lock"]),
        .testTarget(name: "SaveSyncTests", dependencies: ["SaveSync", "EngineRPGMaker", "EngineRenPy"], exclude: ["Fixtures"]),
        .testTarget(name: "ArchiveImportTests", dependencies: ["ArchiveImport"], exclude: ["Fixtures"]),
    ]
)
