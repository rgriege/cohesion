void editor_init(void);
void editor_edit_map(struct map *map);
void editor_update(gui_t *gui, b32 *done);



/* Implementation */

#define EDITOR_REPEAT_INTERVAL 150

struct map *editor_map;
struct map editor_map_orig;
struct history editor_history;
enum action editor_last_action;
u32 editor_repeat_timer;
v2i editor_cursor;

void editor_init(void)
{
	editor_map = NULL;
	editor_last_action = ACTION_COUNT;
	history_clear(&editor_history);
}

static
void editor__init_map(struct map map)
{
	static const v2i dim_max = { MAP_DIM_MAX, MAP_DIM_MAX };
	const v2i offset = v2i_scale_inv(v2i_sub(dim_max, map.dim), 2);
	editor_map->dim = dim_max;

	for (s32 i = 0; i < offset.y; ++i)
		for (s32 j = 0; j < editor_map->dim.x; ++j)
			editor_map->tiles[i][j].type = TILE_BLANK;

	for (s32 i = offset.y; i < offset.y + map.dim.y; ++i) {
		for (s32 j = 0; j < offset.x; ++j)
			editor_map->tiles[i][j].type = TILE_BLANK;
		for (s32 j = offset.x; j < offset.x + map.dim.x; ++j)
			editor_map->tiles[i][j] = map.tiles[i - offset.y][j - offset.x];
		for (s32 j = offset.x + map.dim.x; j < editor_map->dim.x; ++j)
			editor_map->tiles[i][j].type = TILE_BLANK;
	}

	for (s32 i = offset.y + map.dim.y; i < editor_map->dim.y; ++i)
		for (s32 j = 0; j < editor_map->dim.x; ++j)
			editor_map->tiles[i][j].type = TILE_BLANK;
}

void editor_edit_map(struct map *map_)
{
	const struct map map = *map_;
	const b32 changed = editor_map != map_;

	editor_map = map_;

	editor__init_map(map);

	if (changed) {
		editor_map_orig = map;

		editor_cursor.x = MAP_DIM_MAX / 2 - 1;
		editor_cursor.y = MAP_DIM_MAX / 2 - 1;

		history_clear(&editor_history);
	}
}

static
b32 editor_attempt_action(enum action action, gui_t *gui)
{
	const u32 frame_milli = gui_frame_time_milli(gui);

	if (!key_down(gui, g_key_bindings[action])) {
		if (editor_last_action == action)
			editor_last_action = ACTION_COUNT;
		return false;
	} else if (editor_last_action == ACTION_COUNT) {
		editor_last_action = action;
		editor_repeat_timer = EDITOR_REPEAT_INTERVAL;
		return true;
	} else if (action != editor_last_action) {
		return false;
	} else if (editor_repeat_timer <= frame_milli) {
		editor_repeat_timer =   EDITOR_REPEAT_INTERVAL
		                      - (frame_milli - editor_repeat_timer);
		return true;
	} else {
		editor_repeat_timer -= frame_milli;
		return false;
	}
}

void editor_update(gui_t *gui, b32 *done)
{
	v2i screen, offset;

	if (key_pressed(gui, KB_F1) && !is_key_bound(KB_F1)) {
		s32 min_i = MAP_DIM_MAX, min_j = MAP_DIM_MAX;
		s32 max_i = 0, max_j = 0;
		struct map map_play;
		for (s32 i = 0; i < editor_map->dim.y; ++i) {
			for (s32 j = 0; j < editor_map->dim.x; ++j) {
				if (editor_map->tiles[i][j].type != TILE_BLANK) {
					min_i = min(min_i, i);
					min_j = min(min_j, j);
					max_i = max(max_i, i);
					max_j = max(max_j, j);
				}
			}
		}
		map_play.dim.x = max_j - min_j + 1;
		map_play.dim.y = max_i - min_i + 1;
		for (s32 i = 0; i < map_play.dim.y; ++i)
			for (s32 j = 0; j < map_play.dim.x; ++j)
				map_play.tiles[i][j] = editor_map->tiles[i + min_i][j + min_j];

		editor_map->dim = map_play.dim;
		memcpy(editor_map->tiles, map_play.tiles, sizeof(map_play.tiles));
		*done = true;
	} else {
		*done = false;
	}

	gui_dim(gui, &screen.x, &screen.y);
	offset = v2i_scale_inv(v2i_sub(screen, v2i_scale(editor_map->dim, TILE_SIZE)), 2);

	if (editor_attempt_action(ACTION_MOVE_UP, gui)) {
		editor_cursor.y = min(editor_cursor.y + 1, MAP_DIM_MAX - 1);
		history_push(&editor_history, ACTION_MOVE_UP, 0);
	} else if (editor_attempt_action(ACTION_MOVE_DOWN, gui)) {
		editor_cursor.y = max(editor_cursor.y - 1, 0);
		history_push(&editor_history, ACTION_MOVE_DOWN, 0);
	} else if (editor_attempt_action(ACTION_MOVE_LEFT, gui)) {
		editor_cursor.x = max(editor_cursor.x - 1, 0);
		history_push(&editor_history, ACTION_MOVE_LEFT, 0);
	} else if (editor_attempt_action(ACTION_MOVE_RIGHT, gui)) {
		editor_cursor.x = min(editor_cursor.x + 1, MAP_DIM_MAX - 1);
		history_push(&editor_history, ACTION_MOVE_RIGHT, 0);
	} else if (editor_attempt_action(ACTION_ROTATE_CW, gui)) {
		editor_map->tiles[editor_cursor.y][editor_cursor.x].type
			= (editor_map->tiles[editor_cursor.y][editor_cursor.x].type + 1) % (TILE_DOOR + 1);
		history_push(&editor_history, ACTION_ROTATE_CW, 0);
	} else if (editor_attempt_action(ACTION_ROTATE_CCW, gui)) {
		editor_map->tiles[editor_cursor.y][editor_cursor.x].type
			= (editor_map->tiles[editor_cursor.y][editor_cursor.x].type + TILE_DOOR) % (TILE_DOOR + 1);
		history_push(&editor_history, ACTION_ROTATE_CCW, 0);
	} else if (editor_attempt_action(ACTION_UNDO, gui)) {
		enum action action;
		u32 num_clones;
		b32 stop = false;
		while (!stop && history_pop(&editor_history, &action, &num_clones)) {
			switch (action) {
			case ACTION_MOVE_UP:
				--editor_cursor.y;
			break;
			case ACTION_MOVE_DOWN:
				++editor_cursor.y;
			break;
			case ACTION_MOVE_LEFT:
				++editor_cursor.x;
			break;
			case ACTION_MOVE_RIGHT:
				--editor_cursor.x;
			break;
			case ACTION_ROTATE_CW:
				editor_map->tiles[editor_cursor.y][editor_cursor.x].type
					= (editor_map->tiles[editor_cursor.y][editor_cursor.x].type + TILE_DOOR) % (TILE_DOOR + 1);
				stop = true;
			break;
			case ACTION_ROTATE_CCW:
				editor_map->tiles[editor_cursor.y][editor_cursor.x].type
					= (editor_map->tiles[editor_cursor.y][editor_cursor.x].type + 1) % (TILE_DOOR + 1);
				stop = true;
			break;
			case ACTION_UNDO:
			case ACTION_RESET:
			case ACTION_COUNT:
			break;
			}
		}
	} else if (editor_attempt_action(ACTION_RESET, gui)) {
		editor__init_map(editor_map_orig);
		history_clear(&editor_history);
	}

	gui_npt(gui, 2, screen.y - TILE_SIZE + 2, screen.x - 4, TILE_SIZE - 4,
	        editor_map->desc, MAP_TIP_MAX - 1, "description", 0);

	for (s32 i = 0; i < editor_map->dim.y; ++i) {
		const s32 y = offset.y + i * TILE_SIZE;
		for (s32 j = 0; j < editor_map->dim.x; ++j) {
			const s32 x = offset.x + j * TILE_SIZE;
			const enum tile_type type = editor_map->tiles[i][j].type;
			const color_t color = { .r=0x32, .g=0x2f, .b=0x2f, .a=0xff };
			gui_rect(gui, x, y, TILE_SIZE, TILE_SIZE,
			         g_tile_fills[type], color);
		}
	}

	gui_npt(gui, 2, 2, screen.x - 4, TILE_SIZE - 4,
	        editor_map->tip, MAP_TIP_MAX - 1, "tip", 0);

	gui_rect(gui, offset.x + editor_cursor.x * TILE_SIZE, offset.y + editor_cursor.y * TILE_SIZE,
	         TILE_SIZE, TILE_SIZE, g_nocolor, g_white);
}
