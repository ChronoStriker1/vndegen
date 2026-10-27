import SwiftUI
import ControllerUI

/// The licenses of third-party code the app ships. Each is a text file in the app's Licenses folder, named for the
/// code it covers. The app's build copies them there.
struct LicensesView: View {
    @Environment(Navigator.self) private var nav
    private let licenses = ThirdPartyLicense.bundled

    var body: some View {
        ScrollView {
            LazyVStack(alignment: .leading, spacing: 28) {
                if licenses.isEmpty {
                    Text("This build lists no third-party licenses.").foregroundStyle(Theme.secondaryText)
                }
                ForEach(licenses) { license in
                    VStack(alignment: .leading, spacing: 10) {
                        Text(license.name).font(.system(size: 17, weight: .bold))
                        Text(license.text)
                            .font(.system(size: 12, design: .monospaced))
                            .foregroundStyle(Theme.secondaryText)
                            .textSelection(.enabled)
                    }
                }
            }
            .padding(.horizontal, 22)
            .padding(.bottom, 22)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .scrollPager(nav.pager)
    }
}

struct ThirdPartyLicense: Identifiable {
    let name: String
    let text: String
    var id: String { name }

    static var bundled: [ThirdPartyLicense] {
        guard let folder = Bundle.main.url(forResource: "Licenses", withExtension: nil),
              let files = try? FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: nil)
        else { return [] }
        return files.filter { $0.pathExtension == "txt" }
            .sorted { $0.lastPathComponent.localizedStandardCompare($1.lastPathComponent) == .orderedAscending }
            .compactMap { file in
                (try? String(contentsOf: file, encoding: .utf8)).map {
                    ThirdPartyLicense(name: file.deletingPathExtension().lastPathComponent, text: $0)
                }
            }
    }
}
