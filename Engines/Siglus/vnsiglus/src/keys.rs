//! Windows virtual-key codes, which VNDegen sends, as each engine's keys.

use reallive::input::Key;
use siglus_scene_vm::runtime::input::VmKey;

pub fn siglus(virtual_key: i32) -> Option<VmKey> {
    Some(match virtual_key {
        0x08 => VmKey::Backspace,
        0x09 => VmKey::Tab,
        0x0D => VmKey::Enter,
        0x10 | 0xA0 | 0xA1 => VmKey::Shift,
        0x11 | 0xA2 | 0xA3 => VmKey::Control,
        0x12 | 0xA4 | 0xA5 => VmKey::Alt,
        0x1B => VmKey::Escape,
        0x20 => VmKey::Space,
        0x23 => VmKey::End,
        0x24 => VmKey::Home,
        0x25 => VmKey::ArrowLeft,
        0x26 => VmKey::ArrowUp,
        0x27 => VmKey::ArrowRight,
        0x28 => VmKey::ArrowDown,
        0x2E => VmKey::Delete,
        0x30..=0x39 => VmKey::Digit((virtual_key - 0x30) as u8),
        0x41..=0x5A => VmKey::Letter(char::from(virtual_key as u8)),
        0x5B | 0x5C => VmKey::Meta,
        0x70..=0x7B => VmKey::F((virtual_key - 0x6F) as u8),
        _ => return None,
    })
}

pub fn reallive(virtual_key: i32) -> Option<Key> {
    Some(match virtual_key {
        0x08 => Key::Backspace,
        0x09 => Key::Tab,
        0x0D => Key::Enter,
        0x10 | 0xA0 | 0xA1 => Key::Shift,
        0x11 | 0xA2 | 0xA3 => Key::Ctrl,
        0x1B => Key::Escape,
        0x20 => Key::Space,
        0x21 => Key::PageUp,
        0x22 => Key::PageDown,
        0x23 => Key::End,
        0x24 => Key::Home,
        0x25 => Key::Left,
        0x26 => Key::Up,
        0x27 => Key::Right,
        0x28 => Key::Down,
        0x30..=0x39 | 0x41..=0x5A => Key::Char(char::from(virtual_key as u8).to_ascii_lowercase()),
        0x70..=0x7B => Key::F((virtual_key - 0x6F) as u8),
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn keys_map_by_virtual_key() {
        assert_eq!(siglus(0x0D), Some(VmKey::Enter));
        assert_eq!(siglus(0x41), Some(VmKey::Letter('A')));
        assert_eq!(siglus(0x35), Some(VmKey::Digit(5)));
        assert_eq!(siglus(0x70), Some(VmKey::F(1)));
        assert_eq!(siglus(0x7B), Some(VmKey::F(12)));
        assert_eq!(siglus(0xA2), Some(VmKey::Control));
        assert_eq!(siglus(0x07), None);
        assert_eq!(reallive(0x11), Some(Key::Ctrl));
        assert_eq!(reallive(0x21), Some(Key::PageUp));
        assert_eq!(reallive(0x41), Some(Key::Char('a')));
        assert_eq!(reallive(0x71), Some(Key::F(2)));
    }
}
