#include "vngd_platform.h"

Key vngd_hid_to_key(int p_usage) {
	if (p_usage >= 4 && p_usage <= 29) {
		return Key((int)Key::A + p_usage - 4);
	}
	if (p_usage >= 30 && p_usage <= 38) {
		return Key((int)Key::KEY_1 + p_usage - 30);
	}
	if (p_usage >= 58 && p_usage <= 69) {
		return Key((int)Key::F1 + p_usage - 58);
	}
	if (p_usage >= 89 && p_usage <= 97) {
		return Key((int)Key::KP_1 + p_usage - 89);
	}
	switch (p_usage) {
		case 39: return Key::KEY_0;
		case 40: return Key::ENTER;
		case 41: return Key::ESCAPE;
		case 42: return Key::BACKSPACE;
		case 43: return Key::TAB;
		case 44: return Key::SPACE;
		case 45: return Key::MINUS;
		case 46: return Key::EQUAL;
		case 47: return Key::BRACKETLEFT;
		case 48: return Key::BRACKETRIGHT;
		case 49: return Key::BACKSLASH;
		case 51: return Key::SEMICOLON;
		case 52: return Key::APOSTROPHE;
		case 53: return Key::QUOTELEFT;
		case 54: return Key::COMMA;
		case 55: return Key::PERIOD;
		case 56: return Key::SLASH;
		case 57: return Key::CAPSLOCK;
		case 70: return Key::PRINT;
		case 71: return Key::SCROLLLOCK;
		case 72: return Key::PAUSE;
		case 73: return Key::INSERT;
		case 74: return Key::HOME;
		case 75: return Key::PAGEUP;
		case 76: return Key::KEY_DELETE;
		case 77: return Key::END;
		case 78: return Key::PAGEDOWN;
		case 79: return Key::RIGHT;
		case 80: return Key::LEFT;
		case 81: return Key::DOWN;
		case 82: return Key::UP;
		case 83: return Key::NUMLOCK;
		case 84: return Key::KP_DIVIDE;
		case 85: return Key::KP_MULTIPLY;
		case 86: return Key::KP_SUBTRACT;
		case 87: return Key::KP_ADD;
		case 88: return Key::KP_ENTER;
		case 98: return Key::KP_0;
		case 99: return Key::KP_PERIOD;
		case 224: return Key::CTRL;
		case 225: return Key::SHIFT;
		case 226: return Key::ALT;
		case 227: return Key::META;
		case 228: return Key::CTRL;
		case 229: return Key::SHIFT;
		case 230: return Key::ALT;
		case 231: return Key::META;
		default: return Key::NONE;
	}
}
