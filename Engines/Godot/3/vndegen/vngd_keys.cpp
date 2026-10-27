#include "vngd_keys.h"

uint32_t vngd_hid_to_key(int p_usage) {
	if (p_usage >= 4 && p_usage <= 29) {
		return KEY_A + p_usage - 4;
	}
	if (p_usage >= 30 && p_usage <= 38) {
		return KEY_1 + p_usage - 30;
	}
	if (p_usage >= 58 && p_usage <= 69) {
		return KEY_F1 + p_usage - 58;
	}
	if (p_usage >= 89 && p_usage <= 97) {
		return KEY_KP_1 + p_usage - 89;
	}
	switch (p_usage) {
		case 39: return KEY_0;
		case 40: return KEY_ENTER;
		case 41: return KEY_ESCAPE;
		case 42: return KEY_BACKSPACE;
		case 43: return KEY_TAB;
		case 44: return KEY_SPACE;
		case 45: return KEY_MINUS;
		case 46: return KEY_EQUAL;
		case 47: return KEY_BRACKETLEFT;
		case 48: return KEY_BRACKETRIGHT;
		case 49: return KEY_BACKSLASH;
		case 51: return KEY_SEMICOLON;
		case 52: return KEY_APOSTROPHE;
		case 53: return KEY_QUOTELEFT;
		case 54: return KEY_COMMA;
		case 55: return KEY_PERIOD;
		case 56: return KEY_SLASH;
		case 57: return KEY_CAPSLOCK;
		case 70: return KEY_PRINT;
		case 71: return KEY_SCROLLLOCK;
		case 72: return KEY_PAUSE;
		case 73: return KEY_INSERT;
		case 74: return KEY_HOME;
		case 75: return KEY_PAGEUP;
		case 76: return KEY_DELETE;
		case 77: return KEY_END;
		case 78: return KEY_PAGEDOWN;
		case 79: return KEY_RIGHT;
		case 80: return KEY_LEFT;
		case 81: return KEY_DOWN;
		case 82: return KEY_UP;
		case 83: return KEY_NUMLOCK;
		case 84: return KEY_KP_DIVIDE;
		case 85: return KEY_KP_MULTIPLY;
		case 86: return KEY_KP_SUBTRACT;
		case 87: return KEY_KP_ADD;
		case 88: return KEY_KP_ENTER;
		case 98: return KEY_KP_0;
		case 99: return KEY_KP_PERIOD;
		case 224: case 228: return KEY_CONTROL;
		case 225: case 229: return KEY_SHIFT;
		case 226: case 230: return KEY_ALT;
		case 227: case 231: return KEY_META;
		default: return 0;
	}
}
