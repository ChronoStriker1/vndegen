import SwiftUI
import VNCore

/// The game's own menu inside VNDegen's in-game menu, one level at a time, as `PlayerMenu` tracks it.
struct GameMenuView: View {
    @Environment(\.hubMetrics) private var metrics
    let menu: PlayerMenu
    let showsFocus: Bool
    let onChoose: (GameMenuItem) -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Button(action: menu.gameBack) {
                HStack(spacing: 8) {
                    Image(systemName: "chevron.left").font(.system(size: 15, weight: .bold))
                    Text(menu.gamePath.last?.title ?? "Game menu")
                        .font(.display(17, weight: .heavy))
                        .lineLimit(1)
                }
                .foregroundStyle(.white)
                .frame(minHeight: 34)
                .contentShape(.rect)
            }
            .buttonStyle(.plain)
            .accessibilityLabel(menu.gamePath.isEmpty ? "Back to the VNDegen menu" : "Back")
            ScrollViewReader { proxy in
                ScrollView {
                    VStack(spacing: 4) {
                        ForEach(menu.gameLevel) { item in
                            if item.kind == .separator {
                                Divider().overlay(Theme.stroke).padding(.vertical, 4)
                            } else {
                                row(item).id(item.id)
                            }
                        }
                    }
                    .padding(4)
                }
                .scrollIndicators(.hidden)
                .onChange(of: menu.gameFocus) { _, focus in
                    guard let focus else { return }
                    withAnimation(.easeOut(duration: 0.15)) { proxy.scrollTo(focus) }
                }
            }
            .frame(maxHeight: max(160, metrics.size.height - 170))
            .fixedSize(horizontal: false, vertical: true)
        }
        .frame(width: min(380, max(0, metrics.size.width - 64)))
    }

    private func row(_ item: GameMenuItem) -> some View {
        let focused = showsFocus && menu.gameFocus == item.id
        return Button { onChoose(item) } label: {
            HStack(spacing: 10) {
                mark(item)
                    .frame(width: 18)
                Text(item.title)
                    .font(.system(size: 15, weight: .semibold))
                    .lineLimit(2)
                    .multilineTextAlignment(.leading)
                Spacer(minLength: 8)
                if case .submenu = item.kind {
                    Image(systemName: "chevron.right").font(.system(size: 13, weight: .bold))
                }
            }
            .foregroundStyle(focused ? .black : .white)
            .padding(.horizontal, 14)
            .frame(maxWidth: .infinity, minHeight: 44, alignment: .leading)
            .background(focused ? AnyShapeStyle(.white) : AnyShapeStyle(Theme.surface), in: .rect(cornerRadius: 12, style: .continuous))
            .overlay { RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(focused ? .white : Theme.stroke) }
            .contentShape(.rect)
            .opacity(item.isEnabled ? 1 : 0.4)
        }
        .buttonStyle(.plain)
        .disabled(!item.isEnabled)
        .accessibilityAddTraits(item.isChecked ? .isSelected : [])
        .onHover { if $0 { menu.gameFocus = item.id } }
    }

    @ViewBuilder private func mark(_ item: GameMenuItem) -> some View {
        if item.isChecked {
            Image(systemName: item.isRadio ? "circle.fill" : "checkmark")
                .font(.system(size: item.isRadio ? 8 : 13, weight: .bold))
                .foregroundStyle(Theme.accent)
        }
    }
}
