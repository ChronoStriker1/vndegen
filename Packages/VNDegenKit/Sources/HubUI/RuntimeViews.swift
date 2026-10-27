import ControllerUI
import RuntimePackages
import SwiftUI
import UniformTypeIdentifiers
import VNCore

/// On a game's page, each runtime package the game needs and doesn't have, with buttons to download and install it or
/// to take it from a folder. Nothing downloads until the player asks.
struct RuntimeSection: View {
    @Environment(Navigator.self) private var nav
    let game: Game

    var body: some View {
        let runtimes = nav.runtimes
        let missing = runtimes.missing[game.id] ?? []
        if !missing.isEmpty {
            VStack(alignment: .leading, spacing: 14) {
                SectionLabel(text: "Needed to play", systemImage: "shippingbox")
                ForEach(missing) { package in
                    VStack(alignment: .leading, spacing: 10) {
                        Text(package.name).font(.system(size: 17, weight: .bold))
                        Text("This game uses files from the \(package.name), which it doesn't include. VNDegen can download the official installer from \(package.download.host ?? "its publisher") (\(Self.size(package.size))) and install it for every game that needs it. You can also pick a folder with the RTP installed on a PC.")
                            .font(.system(size: 13))
                            .foregroundStyle(Theme.secondaryText)
                            .fixedSize(horizontal: false, vertical: true)
                        switch runtimes.phases[package.id] {
                        case let .running(progress)?:
                            ProgressView(value: Self.fraction(progress, package)) { Text(Self.status(progress)).font(.system(size: 13)) }
                                .tint(Theme.accent)
                                .frame(maxWidth: 420)
                            HubButton(target: .runtime(package.id, .cancel)) { Text("Cancel") }
                        case let .failed(message)?:
                            Text(message).font(.system(size: 13)).foregroundStyle(Theme.accent).fixedSize(horizontal: false, vertical: true)
                            buttons(package)
                        case nil:
                            buttons(package)
                        }
                    }
                }
            }
        }
    }

    @ViewBuilder
    private func buttons(_ package: RuntimePackage) -> some View {
        HStack(spacing: 12) {
            HubButton(target: .runtime(package.id, .install), kind: .primary) { Label("Download and install", systemImage: "arrow.down.circle") }
            HubButton(target: .runtime(package.id, .importFolder)) { Label("Import from folder", systemImage: "folder") }
        }
        if nav.hub.input.lastInputKind == .gamepad {
            Text("Import from folder opens the folder picker, which needs touch or a mouse.")
                .font(.system(size: 13))
                .foregroundStyle(Theme.tertiaryText)
        }
    }

    static func size(_ bytes: Int64) -> String { ByteCountFormatter.string(fromByteCount: bytes, countStyle: .file) }

    static func fraction(_ progress: RuntimeInstallProgress, _ package: RuntimePackage) -> Double? {
        switch progress {
        case let .downloading(received, total): total > 0 ? Double(received) / Double(total) : nil
        case let .extracting(done, total): total > 0 ? Double(done) / Double(total) : nil
        case .verifying, .installing: nil
        }
    }

    static func status(_ progress: RuntimeInstallProgress) -> String {
        switch progress {
        case let .downloading(received, total): "Downloading, \(size(received)) of \(size(total))"
        case .verifying: "Checking the download"
        case let .extracting(done, total): "Unpacking, \(done) of \(total) files"
        case .installing: "Installing"
        }
    }
}

/// The package's license, which the player accepts before VNDegen installs it, as the official installer asks.
struct RuntimeLicenseSheet: View {
    @Environment(Navigator.self) private var nav
    let request: LicenseRequest

    var body: some View {
        SheetScaffold(title: "License", subtitle: request.package.name) {
            VStack(alignment: .leading, spacing: 16) {
                ScrollView {
                    Text(request.text ?? request.package.license ?? "The installer carries no license text. Its license is on \(request.package.licensePage.host ?? "the publisher's site"). Read it there, then accept it here to install the \(request.package.name), as the official installer asks.")
                        .font(.system(size: 13, design: .monospaced))
                        .foregroundStyle(Theme.secondaryText)
                        .textSelection(.enabled)
                        .frame(maxWidth: .infinity, alignment: .leading)
                }
                .scrollPager(nav.pager)
                Link("Read it on \(request.package.licensePage.host ?? "the publisher's site")", destination: request.package.licensePage)
                    .font(.system(size: 13))
                HStack(spacing: 12) {
                    HubButton(target: .licenseAccept, kind: .primary) { Text("Accept and install") }
                    HubButton(target: .licenseDecline) { Text("Decline") }
                }
            }
            .padding(.horizontal, 22)
            .padding(.bottom, 22)
        }
    }
}

extension View {
    /// The folder picker for a runtime package's Import from folder button.
    func runtimeFolderImporter(_ runtimes: RuntimeInstalls, game: GameID) -> some View {
        fileImporter(isPresented: Binding(get: { runtimes.pickingFolder }, set: { runtimes.pickingFolder = $0 }),
                     allowedContentTypes: [.folder]) { result in
            guard let package = runtimes.folderImport else { return }
            runtimes.folderImport = nil
            if case let .success(url) = result { runtimes.importFolder(url, as: package, for: game) }
        }
    }
}
