import Foundation
import SwiftUI
import VNCore
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

@MainActor
func askForText(title: String, message: String, current: String, over view: PlatformView, done: @escaping (String?) -> Void) {
    #if canImport(UIKit)
    var presenter = view.window?.rootViewController
    while let shown = presenter?.presentedViewController { presenter = shown }
    guard let presenter else { return done(nil) }
    let alert = UIAlertController(title: title, message: message, preferredStyle: .alert)
    alert.addTextField { $0.text = current }
    alert.addAction(UIAlertAction(title: "キャンセル", style: .cancel) { _ in done(nil) })
    alert.addAction(UIAlertAction(title: "OK", style: .default) { [weak alert] _ in done(alert?.textFields?.first?.text ?? "") })
    presenter.present(alert, animated: true)
    #else
    guard let window = view.window else { return done(nil) }
    let alert = NSAlert()
    alert.messageText = title
    alert.informativeText = message
    let field = NSTextField(string: current)
    field.frame = NSRect(x: 0, y: 0, width: 300, height: 24)
    alert.accessoryView = field
    alert.addButton(withTitle: "OK")
    alert.addButton(withTitle: "キャンセル")
    alert.window.initialFirstResponder = field
    alert.beginSheetModal(for: window) { response in done(response == .alertFirstButtonReturn ? field.stringValue : nil) }
    #endif
}

struct LicenceView: View {
    let title: String
    let text: String
    let answer: (Bool) -> Void

    var body: some View {
        VStack(spacing: 12) {
            Text(title).font(.headline)
            ScrollView {
                Text(text)
                    .font(.system(size: 13))
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .textSelection(.enabled)
                    .padding(8)
            }
            .background(Color.white)
            .border(Color.gray.opacity(0.5))
            HStack(spacing: 24) {
                Button("同意して続ける") { answer(true) }
                    .keyboardShortcut(.defaultAction)
                Button("同意しない") { answer(false) }
                    .keyboardShortcut(.cancelAction)
            }
            .buttonStyle(.bordered)
        }
        .padding(16)
        .frame(minWidth: 360, idealWidth: 460, minHeight: 420, idealHeight: 520)
        .environment(\.colorScheme, .light)
        .background(Color(white: 0.94))
    }

    nonisolated static func plainText(of url: URL) -> String {
        let options: [NSAttributedString.DocumentReadingOptionKey: Any] = [.documentType: NSAttributedString.DocumentType.rtf]
        return (try? NSAttributedString(url: url, options: options, documentAttributes: nil))?.string ?? ""
    }
}

@MainActor
func askToAcceptLicence(title: String, file: URL, over view: PlatformView, done: @escaping (Bool) -> Void) {
    let text = LicenceView.plainText(of: file)
    #if canImport(UIKit)
    var presenter = view.window?.rootViewController
    while let shown = presenter?.presentedViewController { presenter = shown }
    guard let presenter else { return done(true) }
    let controller = UIHostingController(rootView: LicenceView(title: title, text: text) { [weak presenter] accepted in
        presenter?.dismiss(animated: true)
        done(accepted)
    })
    controller.modalPresentationStyle = .formSheet
    controller.isModalInPresentation = true
    presenter.present(controller, animated: true)
    #else
    guard let window = view.window else { return done(true) }
    let panel = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 460, height: 520), styleMask: [.titled], backing: .buffered, defer: false)
    panel.title = title
    panel.contentViewController = NSHostingController(rootView: LicenceView(title: title, text: text) { [weak window, weak panel] accepted in
        if let panel { window?.endSheet(panel) }
        done(accepted)
    })
    window.beginSheet(panel)
    #endif
}
