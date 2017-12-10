enum action
{
	ACTION_MOVE_UP,
	ACTION_MOVE_DOWN,
	ACTION_MOVE_LEFT,
	ACTION_MOVE_RIGHT,
	ACTION_ROTATE_CW,
	ACTION_ROTATE_CCW,
	ACTION_UNDO,
	ACTION_RESET,
	ACTION_COUNT,
};

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
