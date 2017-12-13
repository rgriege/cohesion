#include "config.h"
#include "violet/all.h"
#include "action.h"
#include "settings.h"

enum action action_last = ACTION_COUNT;
u32 action_repeat_timer;

const char *action_to_string(enum action action)
{
	switch (action) {
	case ACTION_MOVE_UP:    return "Move up";
	case ACTION_MOVE_DOWN:  return "Move down";
	case ACTION_MOVE_LEFT:  return "Move left";
	case ACTION_MOVE_RIGHT: return "Move right";
	case ACTION_ROTATE_CW:  return "Rotate clockwise";
	case ACTION_ROTATE_CCW: return "Rotate counter-clockwise";
	case ACTION_UNDO:       return "Undo";
	case ACTION_RESET:      return "Reset";
	case ACTION_COUNT:
	default:                return "<null>";
	}
}

b32 action_attempt(enum action action, gui_t *gui)
{
	const u32 frame_milli = gui_frame_time_milli(gui);

	if (gui_any_widget_has_focus(gui)) {
		return false;
	} else if (!key_down(gui, g_key_bindings[action])) {
		if (action_last == action)
			action_last = ACTION_COUNT;
		return false;
	} else if (action_last == ACTION_COUNT) {
		action_last = action;
		action_repeat_timer = ACTION_REPEAT_INTERVAL;
		return true;
	} else if (action != action_last) {
		return false;
	} else if (action_repeat_timer <= frame_milli) {
		action_repeat_timer =   ACTION_REPEAT_INTERVAL
		                      - (frame_milli - action_repeat_timer);
		return true;
	} else {
		action_repeat_timer -= frame_milli;
		return false;
	}
}
