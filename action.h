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

const char *action_to_string(enum action action);
b32         action_attempt(enum action action, gui_t *gui);
