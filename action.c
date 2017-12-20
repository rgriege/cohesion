#include "violet/all.h"
#include "action.h"

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

b32 action_is_solo(enum action action)
{
	switch (action) {
	case ACTION_MOVE_UP:
	case ACTION_MOVE_DOWN:
	case ACTION_MOVE_LEFT:
	case ACTION_MOVE_RIGHT:
	case ACTION_ROTATE_CCW:
	case ACTION_ROTATE_CW:
	case ACTION_UNDO:
		return true;
	case ACTION_RESET:
	case ACTION_COUNT:
		return false;
	}
	assert(false);
	return false;
}
