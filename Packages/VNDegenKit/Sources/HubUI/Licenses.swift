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
                        Text(license.reflowed)
                            .font(.system(size: 13))
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

    /// The text with each paragraph's hard line breaks joined, so it wraps to the screen rather than breaking twice
    /// on a narrow one. A line joins the one before when it carries on at the same or a deeper indent. Blank lines,
    /// list items, and rules such as "=====" keep their breaks. The words are unchanged.
    var reflowed: String { Self.reflow(text) }

    nonisolated static func reflow(_ text: String) -> String {
        var result = ""
        var previous: (indent: Int, rule: Bool)?
        for line in text.split(separator: "\n", omittingEmptySubsequences: false) {
            let trimmed = line.trimmingCharacters(in: .whitespaces)
            let indent = line.prefix { $0 == " " || $0 == "\t" }.count
            let rule = !trimmed.isEmpty && trimmed.allSatisfy { "=-_*#~".contains($0) }
            let joins = previous.map { indent >= $0.indent && !$0.rule } == true && !trimmed.isEmpty && !rule
                && !startsList(trimmed)
            if !result.isEmpty { result += joins ? " " : "\n" }
            result += joins ? trimmed : String(line)
            previous = trimmed.isEmpty ? nil : (indent, rule)
        }
        return result
    }

    /// "-", "*", "•", "1.", "1.1.", "(a)" and the like.
    private nonisolated static func startsList(_ line: String) -> Bool {
        guard let first = line.first else { return false }
        if "-*•".contains(first) { return true }
        if first == "(", let close = line.firstIndex(of: ")") {
            let inner = line[line.index(after: line.startIndex)..<close]
            return (1...3).contains(inner.count) && inner.allSatisfy { $0.isNumber || $0.isLowercase }
        }
        let marker = line.prefix { $0.isNumber || $0 == "." || $0.isLetter && $0.isLowercase }
        return marker.contains { $0.isNumber } && marker.count <= 6 && line.dropFirst(marker.count).first.map { $0 == " " || $0 == ")" } == true
            || marker.count == 1 && line.dropFirst().first.map { ".)".contains($0) } == true
    }

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
