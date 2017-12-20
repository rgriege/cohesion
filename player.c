#include "config.h"
#include "violet/all.h"
#include "action.h"
#include "types.h"
#include "player.h"
#include "history.h"
#include "settings.h"

void player_init(struct player *player, u32 idx)
{
	player->num_actors = 0;
	player->key_bindings = g_key_bindings[idx];
	player->last_action = ACTION_COUNT;
	player->pending_action = ACTION_COUNT;
	history_clear(&player->history);
}

b32 player_desires_action(struct player *player, enum action action,
                          const gui_t *gui)
{
	const u32 frame_milli = gui_frame_time_milli(gui);

	if (gui_any_widget_has_focus(gui)) {
		return false;
	} else if (!key_down(gui, player->key_bindings[action])) {
		if (player->last_action == action)
			player->last_action = ACTION_COUNT;
		return false;
	} else if (player->last_action == ACTION_COUNT) {
		player->last_action = action;
		player->action_repeat_timer = ACTION_REPEAT_INTERVAL;
		return true;
	} else if (action != player->last_action) {
		return false;
	} else if (player->action_repeat_timer <= frame_milli) {
		player->action_repeat_timer =   ACTION_REPEAT_INTERVAL
		                              - (frame_milli - player->action_repeat_timer);
		return true;
	} else {
		player->action_repeat_timer -= frame_milli;
		return false;
	}
}

enum action player_desired_action(struct player *player, const gui_t *gui)
{
	for (u32 j = 0; j < player->num_actors; ++j)
		if (player->actors[j]->dir != DIR_NONE)
			return ACTION_COUNT;

	if (key_down(gui, player->key_bindings[ACTION_MOVE_UP]))
		return ACTION_MOVE_UP;
	else if (key_down(gui, player->key_bindings[ACTION_MOVE_DOWN]))
		return ACTION_MOVE_DOWN;
	else if (key_down(gui, player->key_bindings[ACTION_MOVE_LEFT]))
		return ACTION_MOVE_LEFT;
	else if (key_down(gui, player->key_bindings[ACTION_MOVE_RIGHT]))
		return ACTION_MOVE_RIGHT;
	else if (player_desires_action(player, ACTION_ROTATE_CCW, gui))
		return ACTION_ROTATE_CCW;
	else if (player_desires_action(player, ACTION_ROTATE_CW, gui))
		return ACTION_ROTATE_CW;
	else if (player_desires_action(player, ACTION_UNDO, gui))
		return ACTION_UNDO;
	else if (key_pressed(gui, player->key_bindings[ACTION_RESET]))
		return ACTION_RESET;
	else
		return ACTION_COUNT;
}
