import SwiftUI
import VNCore

/// A button bound to a focus target, so touch, pointer, and controller all reach the same action and show the same focus.
struct HubButton<Label: View>: View {
    @Environment(Navigator.self) private var nav
    let target: Navigator.Target
    var kind: HubButtonStyle.Kind = .secondary
    @ViewBuilder var label: Label

    var body: some View {
        Button { nav.select(target) } label: { label }
            .buttonStyle(HubButtonStyle(kind: kind, focused: nav.isFocused(target)))
            .onHover { if $0 { nav.hover(target) } }
            .id(target)
    }
}

struct HubButtonStyle: ButtonStyle {
    enum Kind { case primary, secondary, icon, destructive }
    let kind: Kind
    let focused: Bool

    func makeBody(configuration: Configuration) -> some View {
        let shape = kind == .icon ? AnyShape(Circle()) : AnyShape(Capsule())
        configuration.label
            .font(.system(size: kind == .primary ? 17 : 15, weight: .bold))
            .foregroundStyle(kind == .primary || kind == .destructive ? .white : (focused ? .black : .white))
            .padding(.horizontal, kind == .icon ? 0 : (kind == .primary ? 28 : 18))
            .frame(minWidth: kind == .icon ? 44 : nil, minHeight: kind == .primary ? 52 : 44)
            .background {
                switch kind {
                case .primary: shape.fill(Theme.accent.gradient)
                case .destructive: shape.fill(Color.red.opacity(focused ? 1 : 0.8))
                case .secondary, .icon:
                    if focused { shape.fill(.white) } else { shape.fill(.ultraThinMaterial).overlay(shape.fill(.white.opacity(0.06))) }
                }
            }
            .overlay { shape.stroke(focused ? .white : Theme.stroke, lineWidth: focused ? (kind == .primary || kind == .destructive ? 3 : 0) : 1) }
            .contentShape(shape)
            .scaleEffect(configuration.isPressed ? 0.95 : (focused ? 1.06 : 1))
            .shadow(color: focused ? Theme.accent.opacity(kind == .primary ? 0.7 : 0.35) : .clear, radius: 16)
            .animation(Theme.focusAnimation, value: focused)
            .animation(Theme.focusAnimation, value: configuration.isPressed)
    }
}

/// A portrait library tile. Focus lifts it with a white ring and accent glow.
struct GameTile: View {
    @Environment(Navigator.self) private var nav
    @Environment(\.hubMetrics) private var metrics
    let game: Game
    let target: Navigator.Target
    var width: CGFloat?

    var body: some View {
        let focused = nav.isFocused(target)
        let width = width ?? metrics.tileWidth
        Button { nav.select(target) } label: {
            GameArt(game: game, slots: [.capsule], size: CGSize(width: width, height: width * 1.5))
                .clipShape(.rect(cornerRadius: 10, style: .continuous))
                .overlay(alignment: .topLeading) { LockBadge(game: game) }
                .unavailable(nav.hub.library.unavailableReason(game.id) != nil)
        }
        .buttonStyle(TileButtonStyle(focused: focused))
        .gameMenu(game.id)
        .onHover { if $0 { nav.hover(target) } }
        .id(target)
        .accessibilityLabel(nav.hub.displayTitle(game))
    }
}

/// A landscape library tile with the game's header art. A game without a header shows its hero with the logo on top,
/// since a hero has no title of its own.
struct HeaderTile: View {
    @Environment(Hub.self) private var hub
    @Environment(Navigator.self) private var nav
    let game: Game
    let target: Navigator.Target
    let width: CGFloat

    var body: some View {
        let header = ArtworkSlot.header.nominalSize
        let size = CGSize(width: width, height: (width * header.height / header.width).rounded())
        Button { nav.select(target) } label: {
            Group {
                if hub.artworkURL(game, [.header]) != nil {
                    GameArt(game: game, slots: [.header], size: size)
                } else {
                    ZStack {
                        GameArt(game: game, slots: [.hero], size: size)
                        GameLogo(game: game, maxSize: CGSize(width: size.width * 0.7, height: size.height * 0.6), alignment: .center)
                    }
                }
            }
            .clipShape(.rect(cornerRadius: 10, style: .continuous))
            .overlay(alignment: .topLeading) { LockBadge(game: game) }
            .unavailable(hub.library.unavailableReason(game.id) != nil)
        }
        .buttonStyle(TileButtonStyle(focused: nav.isFocused(target)))
        .gameMenu(game.id)
        .onHover { if $0 { nav.hover(target) } }
        .id(target)
        .accessibilityLabel(hub.displayTitle(game))
    }
}

/// A library tile's menu for a long press or a right click. A controller's X opens the same actions in a sheet.
private struct GameMenu: ViewModifier {
    @Environment(Navigator.self) private var nav
    let id: GameID

    func body(content: Content) -> some View {
        content.contextMenu {
            ForEach(nav.gameActions(id), id: \.action) { item in
                Button(role: item.action == .remove ? .destructive : nil) {
                    nav.perform(item.action, on: id)
                } label: {
                    Label(item.title, systemImage: item.systemImage)
                }
                if item.action == .metadata || item.action == .showFolder { Divider() }
            }
        }
    }
}

extension View {
    func gameMenu(_ id: GameID) -> some View { modifier(GameMenu(id: id)) }

    /// Dims a tile whose game is in a library folder that can't be reached, and marks it with a drive icon.
    @ViewBuilder
    func unavailable(_ unavailable: Bool) -> some View {
        if unavailable {
            opacity(0.45)
                .overlay(alignment: .topTrailing) {
                    Image(systemName: "externaldrive.badge.xmark")
                        .font(.system(size: 11, weight: .bold))
                        .padding(7)
                        .background(.ultraThinMaterial, in: .circle)
                        .padding(8)
                }
                .accessibilityValue("Unavailable")
        } else {
            self
        }
    }
}

private struct LockBadge: View {
    let game: Game

    var body: some View {
        if game.isLocked {
            Image(systemName: "lock.fill")
                .font(.system(size: 11, weight: .bold))
                .padding(7)
                .background(.ultraThinMaterial, in: .circle)
                .padding(8)
        }
    }
}

struct TileButtonStyle: ButtonStyle {
    let focused: Bool

    func makeBody(configuration: Configuration) -> some View {
        let shape = RoundedRectangle(cornerRadius: 10, style: .continuous)
        configuration.label
            .overlay { shape.strokeBorder(.white.opacity(focused ? 1 : 0.08), lineWidth: focused ? 3 : 1) }
            .scaleEffect(configuration.isPressed ? 0.97 : (focused ? 1.07 : 1))
            // Only the focused tile casts a glow. A shadow on every tile costs an offscreen pass per tile while scrolling.
            .shadow(color: focused ? Theme.accent.opacity(0.55) : .clear, radius: focused ? 22 : 0)
            .zIndex(focused ? 1 : 0)
            .animation(Theme.focusAnimation, value: focused)
            .animation(Theme.focusAnimation, value: configuration.isPressed)
    }
}

/// Small rounded tag, for the engine badge and genres.
struct Tag: View {
    let text: String
    var prominent = false

    var body: some View {
        Text(text)
            .font(.system(size: 12, weight: .semibold))
            .padding(.horizontal, 9)
            .padding(.vertical, 4)
            .foregroundStyle(prominent ? .white : Theme.secondaryText)
            .background(prominent ? AnyShapeStyle(Theme.accent.opacity(0.85)) : AnyShapeStyle(Theme.surface), in: .capsule)
            .overlay { Capsule().strokeBorder(Theme.stroke, lineWidth: prominent ? 0 : 1) }
    }
}
