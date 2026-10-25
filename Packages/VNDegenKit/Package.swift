// swift-tools-version: 6.2
import PackageDescription

let mainActorByDefault: [SwiftSetting] = [.defaultIsolation(MainActor.self)]

let package = Package(
    name: "VNDegenKit",
    platforms: [.iOS(.v18), .macOS(.v15)],
    products: [
        .library(name: "VNCore", targets: ["VNCore"]),
        .library(name: "EngineRPGMaker", targets: ["EngineRPGMaker"]),
        .library(name: "Metadata", targets: ["Metadata"]),
        .library(name: "GameInput", targets: ["GameInput"]),
        .library(name: "Lock", targets: ["Lock"]),
        .library(name: "HubUI", targets: ["HubUI"]),
        .library(name: "SaveSync", targets: ["SaveSync"]),
    ],
    targets: [
        .target(name: "VNCore"),
        .target(name: "GameInput", dependencies: ["VNCore"], swiftSettings: mainActorByDefault),
        .target(name: "EngineRPGMaker", dependencies: ["VNCore"], resources: [.process("Resources")], swiftSettings: mainActorByDefault),
        .target(name: "Metadata", dependencies: ["VNCore"]),
        .target(name: "Lock", dependencies: ["VNCore"], swiftSettings: mainActorByDefault),
        .target(name: "HubUI", dependencies: ["VNCore", "GameInput", "Metadata", "Lock"], swiftSettings: mainActorByDefault),
        .target(name: "SaveSync", dependencies: ["VNCore"]),
        .testTarget(name: "VNCoreTests", dependencies: ["VNCore"]),
        .testTarget(name: "EngineRPGMakerTests", dependencies: ["EngineRPGMaker"]),
        // Tests read fixtures by file path, so SwiftPM must not treat them as resources.
        .testTarget(name: "MetadataTests", dependencies: ["Metadata"], exclude: ["Fixtures"]),
        .testTarget(name: "GameInputTests", dependencies: ["GameInput"]),
        .testTarget(name: "LockTests", dependencies: ["Lock"]),
        .testTarget(name: "SaveSyncTests", dependencies: ["SaveSync", "EngineRPGMaker"], exclude: ["Fixtures"]),
    ]
)
