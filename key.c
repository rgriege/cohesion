#include "violet/all.h"

b32 is_key(u32 idx)
{
	switch (idx) {
	case KB_A:
	case KB_B:
	case KB_C:
	case KB_D:
	case KB_E:
	case KB_F:
	case KB_G:
	case KB_H:
	case KB_I:
	case KB_J:
	case KB_K:
	case KB_L:
	case KB_M:
	case KB_N:
	case KB_O:
	case KB_P:
	case KB_Q:
	case KB_R:
	case KB_S:
	case KB_T:
	case KB_U:
	case KB_V:
	case KB_W:
	case KB_X:
	case KB_Y:
	case KB_Z:

	case KB_1:
	case KB_2:
	case KB_3:
	case KB_4:
	case KB_5:
	case KB_6:
	case KB_7:
	case KB_8:
	case KB_9:
	case KB_0:

	case KB_RETURN:
	case KB_ESCAPE:
	case KB_BACKSPACE:
	case KB_TAB:
	case KB_SPACE:

	case KB_MINUS:
	case KB_EQUALS:
	case KB_LEFTBRACKET:
	case KB_RIGHTBRACKET:
	case KB_BACKSLASH:
	case KB_SEMICOLON:
	case KB_APOSTROPHE:
	case KB_GRAVE:
	case KB_COMMA:
	case KB_PERIOD:
	case KB_SLASH:

	case KB_CAPSLOCK:

	case KB_F1:
	case KB_F2:
	case KB_F3:
	case KB_F4:
	case KB_F5:
	case KB_F6:
	case KB_F7:
	case KB_F8:
	case KB_F9:
	case KB_F10:
	case KB_F11:
	case KB_F12:

	case KB_PRINTSCREEN:
	case KB_SCROLLLOCK:
	case KB_PAUSE:
	case KB_INSERT:
	case KB_HOME:
	case KB_PAGEUP:
	case KB_DELETE:
	case KB_END:
	case KB_PAGEDOWN:
	case KB_RIGHT:
	case KB_LEFT:
	case KB_DOWN:
	case KB_UP:
	case KB_NUMLOCK_OR_CLEAR:
	case KB_KP_DIVIDE:
	case KB_KP_MULTIPLY:
	case KB_KP_MINUS:
	case KB_KP_PLUS:
	case KB_KP_ENTER:
	case KB_KP_1:
	case KB_KP_2:
	case KB_KP_3:
	case KB_KP_4:
	case KB_KP_5:
	case KB_KP_6:
	case KB_KP_7:
	case KB_KP_8:
	case KB_KP_9:
	case KB_KP_0:
	case KB_KP_PERIOD:
	case KB_KP_EQUALS:
	case KB_F13:
	case KB_F14:
	case KB_F15:
	case KB_F16:
	case KB_F17:
	case KB_F18:
	case KB_F19:
	case KB_F20:
	case KB_F21:
	case KB_F22:
	case KB_F23:
	case KB_F24:
	case KB_LCTRL:
	case KB_LSHIFT:
	case KB_LALT:
	case KB_RCTRL:
	case KB_RSHIFT:
	case KB_RALT:
		return true;
	}
	return false;
}

const char *key_to_string(gui_key_t key)
{
	switch (key) {
	case KB_A:                return "A";
	case KB_B:                return "B";
	case KB_C:                return "C";
	case KB_D:                return "D";
	case KB_E:                return "E";
	case KB_F:                return "F";
	case KB_G:                return "G";
	case KB_H:                return "H";
	case KB_I:                return "I";
	case KB_J:                return "J";
	case KB_K:                return "K";
	case KB_L:                return "L";
	case KB_M:                return "M";
	case KB_N:                return "N";
	case KB_O:                return "O";
	case KB_P:                return "P";
	case KB_Q:                return "Q";
	case KB_R:                return "R";
	case KB_S:                return "S";
	case KB_T:                return "T";
	case KB_U:                return "U";
	case KB_V:                return "V";
	case KB_W:                return "W";
	case KB_X:                return "X";
	case KB_Y:                return "Y";
	case KB_Z:                return "Z";

	case KB_1:                return "1";
	case KB_2:                return "2";
	case KB_3:                return "3";
	case KB_4:                return "4";
	case KB_5:                return "5";
	case KB_6:                return "6";
	case KB_7:                return "7";
	case KB_8:                return "8";
	case KB_9:                return "9";
	case KB_0:                return "0";

	case KB_RETURN:           return "Enter";
	case KB_ESCAPE:           return "Escape";
	case KB_BACKSPACE:        return "Backspace";
	case KB_TAB:              return "Tab";
	case KB_SPACE:            return "Space";

	case KB_MINUS:            return "-";
	case KB_EQUALS:           return "=";
	case KB_LEFTBRACKET:      return "[";
	case KB_RIGHTBRACKET:     return "]";
	case KB_BACKSLASH:        return "\\";
	case KB_SEMICOLON:        return ";";
	case KB_APOSTROPHE:       return "'";
	case KB_GRAVE:            return "`";
	case KB_COMMA:            return ",";
	case KB_PERIOD:           return ".";
	case KB_SLASH:            return "/";

	case KB_CAPSLOCK:         return "Capslock";

	case KB_F1:               return "F1";
	case KB_F2:               return "F2";
	case KB_F3:               return "F3";
	case KB_F4:               return "F4";
	case KB_F5:               return "F5";
	case KB_F6:               return "F6";
	case KB_F7:               return "F7";
	case KB_F8:               return "F8";
	case KB_F9:               return "F9";
	case KB_F10:              return "F10";
	case KB_F11:              return "F11";
	case KB_F12:              return "F12";

	case KB_PRINTSCREEN:      return "Print Screen";
	case KB_SCROLLLOCK:       return "Scroll Lock";
	case KB_PAUSE:            return "Pause";
	case KB_INSERT:           return "Insert";
	case KB_HOME:             return "Home";
	case KB_PAGEUP:           return "Page Up";
	case KB_DELETE:           return "Delete";
	case KB_END:              return "End";
	case KB_PAGEDOWN:         return "Page Down";
	case KB_RIGHT:            return "Right";
	case KB_LEFT:             return "Left";
	case KB_DOWN:             return "Down";
	case KB_UP:               return "Up";
	case KB_NUMLOCK_OR_CLEAR: return "Numlock";
	case KB_KP_DIVIDE:        return "Keypad /";
	case KB_KP_MULTIPLY:      return "Keypad *";
	case KB_KP_MINUS:         return "Keypad -";
	case KB_KP_PLUS:          return "Keypad +";
	case KB_KP_ENTER:         return "Keypad Enter";
	case KB_KP_1:             return "Keypad 1";
	case KB_KP_2:             return "Keypad 2";
	case KB_KP_3:             return "Keypad 3";
	case KB_KP_4:             return "Keypad 4";
	case KB_KP_5:             return "Keypad 5";
	case KB_KP_6:             return "Keypad 6";
	case KB_KP_7:             return "Keypad 7";
	case KB_KP_8:             return "Keypad 8";
	case KB_KP_9:             return "Keypad 9";
	case KB_KP_0:             return "Keypad 0";
	case KB_KP_PERIOD:        return "Keypad .";
	case KB_KP_EQUALS:        return "Keypad =";
	case KB_F13:              return "F13";
	case KB_F14:              return "F14";
	case KB_F15:              return "F15";
	case KB_F16:              return "F16";
	case KB_F17:              return "F17";
	case KB_F18:              return "F18";
	case KB_F19:              return "F19";
	case KB_F20:              return "F20";
	case KB_F21:              return "F21";
	case KB_F22:              return "F22";
	case KB_F23:              return "F23";
	case KB_F24:              return "F24";
	case KB_LCTRL:            return "Left Control";
	case KB_LSHIFT:           return "Left Shift";
	case KB_LALT:             return "Left Alt";
	case KB_RCTRL:            return "Right Control";
	case KB_RSHIFT:           return "Right Shift";
	case KB_RALT:             return "Right Alt";
	case KB_UNKNOWN:
	case KB_COUNT:
	default:                  return "<null>";
	}
}
