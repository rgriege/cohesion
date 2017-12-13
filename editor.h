void editor_init(void);
void editor_edit_map(array(struct map) *maps, u32 idx);
void editor_update(gui_t *gui, u32 *map_to_play);



/* Implementation */

#define EDITOR_REPEAT_INTERVAL 150

u32 editor_map_idx;
array(struct map) *editor_maps;
struct map editor_map_orig;
struct history editor_history;
enum action editor_last_action;
u32 editor_repeat_timer;
v2i editor_cursor;

void editor_init(void)
{
	editor_maps = NULL;
	editor_map_idx = ~0;
	editor_last_action = ACTION_COUNT;
	history_clear(&editor_history);
}

static
void editor__init_map(struct map map)
{
	static const v2i dim_max = { MAP_DIM_MAX, MAP_DIM_MAX };
	const v2i offset = v2i_scale_inv(v2i_sub(dim_max, map.dim), 2);
	struct map *editor_map = &(*editor_maps)[editor_map_idx];

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

void editor_edit_map(array(struct map) *maps, u32 idx)
{
	const b32 changed = editor_map_idx != idx;
	const struct map map = (*maps)[idx];

	editor_maps = maps;
	editor_map_idx = idx;

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

	if (gui_any_widget_has_focus(gui)) {
		return false;
	} else if (!key_down(gui, g_key_bindings[action])) {
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

static
b32 editor__wall_needed_at_tile(const struct map *map, s32 i, s32 j)
{
	b32 wall_needed = false;
	for (s32 ii = max(i - 1, 0); ii < min(i + 2, map->dim.y); ++ii)
		for (s32 jj = max(j - 1, 0); jj < min(j + 2, map->dim.x); ++jj)
			wall_needed |=    !(ii == i && jj == j)
			               && map->tiles[ii][jj].type != TILE_WALL
			               && map->tiles[ii][jj].type != TILE_BLANK;
	return wall_needed;
}

static
void editor__cursor_move_to(s32 i, s32 j, u32 *num_moves_)
{
	u32 num_moves = 0;
	while (editor_cursor.y < i) {
		history_push(&editor_history, ACTION_MOVE_UP, 0);
		++editor_cursor.y;
		++num_moves;
	}
	while (editor_cursor.y > i) {
		history_push(&editor_history, ACTION_MOVE_DOWN, 0);
		--editor_cursor.y;
		++num_moves;
	}
	while (editor_cursor.x > j) {
		history_push(&editor_history, ACTION_MOVE_LEFT, 0);
		--editor_cursor.x;
		++num_moves;
	}
	while (editor_cursor.x < j) {
		history_push(&editor_history, ACTION_MOVE_RIGHT, 0);
		++editor_cursor.x;
		++num_moves;
	}
	if (num_moves_)
		*num_moves_ = num_moves;
}

static
void editor__rotate_tile_cw(struct map *map)
{
	const s32 i = editor_cursor.y;
	const s32 j = editor_cursor.x;
	map->tiles[i][j].type = (map->tiles[i][j].type + 1) % (TILE_DOOR + 1);
}

static
void editor__rotate_tile_ccw(struct map *map)
{
	const s32 i = editor_cursor.y;
	const s32 j = editor_cursor.x;
	map->tiles[i][j].type = (map->tiles[i][j].type + TILE_DOOR) % (TILE_DOOR + 1);
}

v2i editor__map_dim(const struct map *map, v2i *min)
{
	s32 min_i = MAP_DIM_MAX, min_j = MAP_DIM_MAX;
	s32 max_i = 0, max_j = 0;

	for (s32 i = 0; i < map->dim.y; ++i) {
		for (s32 j = 0; j < map->dim.x; ++j) {
			if (map->tiles[i][j].type != TILE_BLANK) {
				min_i = min(min_i, i);
				min_j = min(min_j, j);
				max_i = max(max_i, i);
				max_j = max(max_j, j);
			}
		}
	}

	if (min) {
		min->x = min_j;
		min->y = min_i;
	}
	return (v2i){
		.x = max(max_j - min_j + 1, 0),
		.y = max(max_i - min_i + 1, 0),
	};
}

static
void editor__cleanup_map_border(struct map *map)
{
	const v2i orig_cursor = editor_cursor;
	u32 change_cnt = 0;

	for (s32 i = 0; i < map->dim.y; ++i) {
		for (s32 j = 0; j < map->dim.x; ++j) {
			switch (map->tiles[i][j].type) {
			case TILE_BLANK:
				if (editor__wall_needed_at_tile(map, i, j)) {
					u32 num_moves;
					editor__cursor_move_to(i, j, &num_moves);
					change_cnt += num_moves;

					while (map->tiles[i][j].type != TILE_WALL) {
						editor__rotate_tile_cw(map);
						history_push(&editor_history, ACTION_ROTATE_CW, 0);
						++change_cnt;
					}
				}
			break;
			case TILE_WALL:
				if (!editor__wall_needed_at_tile(map, i, j)) {
					u32 num_moves;
					editor__cursor_move_to(i, j, &num_moves);
					change_cnt += num_moves;

					while (map->tiles[i][j].type != TILE_BLANK) {
						editor__rotate_tile_ccw(map);
						history_push(&editor_history, ACTION_ROTATE_CCW, 0);
						++change_cnt;
					}
				}
			break;
			case TILE_HALL:
			case TILE_PLAYER:
			case TILE_CLONE:
			case TILE_DOOR:
			break;
			}
		}
	}

	{
		u32 num_moves;
		editor__cursor_move_to(orig_cursor.y, orig_cursor.x, &num_moves);
		change_cnt += num_moves;
	}

	if (change_cnt)
		history_push(&editor_history, ACTION_COUNT, change_cnt);
}

static
void editor__restore_map(void)
{
	struct map *editor_map = &(*editor_maps)[editor_map_idx];
	struct map map_play;
	v2i min;

	editor__cleanup_map_border(editor_map);

	map_play.dim = editor__map_dim(editor_map, &min);

	if (!v2i_equal(map_play.dim, g_v2i_zero)) {
		for (s32 i = 0; i < map_play.dim.y; ++i)
			for (s32 j = 0; j < map_play.dim.x; ++j)
				map_play.tiles[i][j] = editor_map->tiles[i + min.y][j + min.x];

		editor_map->dim = map_play.dim;
		memcpy(editor_map->tiles, map_play.tiles, sizeof(map_play.tiles));
	} else {
		editor_map->dim = g_v2i_zero;
	}
}

static
b32 editor__map_is_blank(const struct map *map)
{
	for (s32 i = 0; i < map->dim.y; ++i)
		for (s32 j = 0; j < map->dim.x; ++j)
			if (map->tiles[i][j].type != TILE_BLANK)
				return false;
	return true;
}

void editor_update(gui_t *gui, u32 *map_to_play)
{
	struct map *editor_map = &(*editor_maps)[editor_map_idx];
	v2i screen, offset;

	if (   key_pressed(gui, KB_F1)
	    && !is_key_bound(KB_F1)
	    && !editor__map_is_blank(editor_map)) {
		editor__restore_map();
		*map_to_play = editor_map_idx;
	} else {
		*map_to_play = ~0;
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
		editor__rotate_tile_cw(editor_map);
		history_push(&editor_history, ACTION_ROTATE_CW, 0);
	} else if (editor_attempt_action(ACTION_ROTATE_CCW, gui)) {
		editor__rotate_tile_ccw(editor_map);
		history_push(&editor_history, ACTION_ROTATE_CCW, 0);
	} else if (editor_attempt_action(ACTION_UNDO, gui)) {
		enum action action;
		u32 num_clones;
		u32 remaining = 1;
		b32 count_moves = false;
		while (remaining && history_pop(&editor_history, &action, &num_clones)) {
			switch (action) {
			case ACTION_MOVE_UP:
				--editor_cursor.y;
				remaining -= count_moves;
			break;
			case ACTION_MOVE_DOWN:
				++editor_cursor.y;
				remaining -= count_moves;
			break;
			case ACTION_MOVE_LEFT:
				++editor_cursor.x;
				remaining -= count_moves;
			break;
			case ACTION_MOVE_RIGHT:
				--editor_cursor.x;
				remaining -= count_moves;
			break;
			case ACTION_ROTATE_CW:
				editor__rotate_tile_ccw(editor_map);
				--remaining;
			break;
			case ACTION_ROTATE_CCW:
				editor__rotate_tile_cw(editor_map);
				--remaining;
			break;
			case ACTION_UNDO:
			case ACTION_RESET:
			case ACTION_COUNT:
				remaining = num_clones;
				count_moves = true;
			break;
			}
		}
	} else if (editor_attempt_action(ACTION_RESET, gui)) {
		editor__init_map(editor_map_orig);
		history_clear(&editor_history);
	} else if (key_pressed(gui, key_next)) {
		editor__restore_map();
		if (key_mod(gui, KBM_CTRL)) {
			const struct map map_copy = *editor_map;
			++editor_map_idx;
			array_insert(*editor_maps, editor_map_idx, map_copy);
		} else if (editor_map_idx + 1 == array_sz(*editor_maps)) {
			if (editor__map_is_blank(editor_map)) {
				array_pop(*editor_maps);
				editor_map_idx = 0;
			} else {
				const struct map map_empty = { 0 };
				array_append(*editor_maps, map_empty);
				++editor_map_idx;
			}
		} else {
			++editor_map_idx;
		}
		history_clear(&editor_history);
		editor_map = &(*editor_maps)[editor_map_idx];
		editor__init_map((*editor_maps)[editor_map_idx]);
	} else if (key_pressed(gui, key_prev)) {
		editor__restore_map();
		if (editor_map_idx == 0) {
			const struct map map_empty = { 0 };
			editor_map_idx = array_sz(*editor_maps);
			array_append(*editor_maps, map_empty);
		} else {
			if (   editor_map_idx + 1 == array_sz(*editor_maps)
			    && editor__map_is_blank(editor_map))
					array_pop(*editor_maps);
			--editor_map_idx;
		}
		history_clear(&editor_history);
		editor_map = &(*editor_maps)[editor_map_idx];
		editor__init_map((*editor_maps)[editor_map_idx]);
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
