#include "config.h"
#include "violet/all.h"
#include "key.h"
#include "action.h"
#include "types.h"
#include "constants.h"
#include "history.h"
#include "settings.h"
#include "player.h"
#include "editor.h"

static u32 editor_map_idx;
static array(struct map) *editor_maps;
static struct map editor_map_orig, editor_map_cut;
static v2i editor_cursor;
static struct player editor_player;

void editor_init(void)
{
	editor_maps = NULL;
	editor_map_idx = ~0;
	editor_map_cut.dim = g_v2i_zero;
	player_init(&editor_player, 0);
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

		player_init(&editor_player, 0);
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
		history_push(&editor_player.history, ACTION_MOVE_UP, 0);
		++editor_cursor.y;
		++num_moves;
	}
	while (editor_cursor.y > i) {
		history_push(&editor_player.history, ACTION_MOVE_DOWN, 0);
		--editor_cursor.y;
		++num_moves;
	}
	while (editor_cursor.x > j) {
		history_push(&editor_player.history, ACTION_MOVE_LEFT, 0);
		--editor_cursor.x;
		++num_moves;
	}
	while (editor_cursor.x < j) {
		history_push(&editor_player.history, ACTION_MOVE_RIGHT, 0);
		++editor_cursor.x;
		++num_moves;
	}
	if (num_moves_)
		*num_moves_ = num_moves;
}

static
u32 editor__tile_actor_idx(const struct map *map, s32 i, s32 j)
{
	u32 actor_idx = 0;
	s32 ii = 0, jj = 0;
	while (!(ii == i && jj == j)) {
		if (map->tiles[ii][jj].type == TILE_ACTOR)
			++actor_idx;
		if (++jj == MAP_DIM_MAX) {
			++ii;
			jj = 0;
		}
	}
	return actor_idx;
}

static
void editor__rotate_tile_cw(struct map *map)
{
	const s32 i = editor_cursor.y;
	const s32 j = editor_cursor.x;
	switch (map->tiles[i][j].type) {
	case TILE_BLANK:
		map->tiles[i][j].type = TILE_HALL;
	break;
	case TILE_WALL:
		map->tiles[i][j].type = TILE_HALL;
	break;
	case TILE_HALL:
		map->tiles[i][j].type = TILE_ACTOR;
		map->actor_controlled_by_player[editor__tile_actor_idx(map, i, j)] = 0;
	break;
	case TILE_ACTOR:;
		const u32 actor_idx = editor__tile_actor_idx(map, i, j);
		if (map->actor_controlled_by_player[actor_idx] == PLAYER_CNT_MAX - 1)
			map->tiles[i][j].type = TILE_CLONE;
		else
			++map->actor_controlled_by_player[actor_idx];
	break;
	case TILE_CLONE:
		map->tiles[i][j].type = TILE_CLONE2;
	break;
	case TILE_DOOR:
		map->tiles[i][j].type = TILE_BLANK;
	break;
	case TILE_CLONE2:
		map->tiles[i][j].type = TILE_DOOR;
	break;
	}
}

static
void editor__rotate_tile_ccw(struct map *map)
{
	const s32 i = editor_cursor.y;
	const s32 j = editor_cursor.x;
	switch (map->tiles[i][j].type) {
	case TILE_BLANK:
		map->tiles[i][j].type = TILE_DOOR;
	break;
	case TILE_WALL:
		map->tiles[i][j].type = TILE_BLANK;
	break;
	case TILE_HALL:
		map->tiles[i][j].type = TILE_BLANK;
	break;
	case TILE_ACTOR:;
		const u32 actor_idx = editor__tile_actor_idx(map, i, j);
		if (map->actor_controlled_by_player[actor_idx] == 0)
			map->tiles[i][j].type = TILE_HALL;
		else
			--map->actor_controlled_by_player[actor_idx];
	break;
	case TILE_CLONE:
		map->tiles[i][j].type = TILE_ACTOR;
		map->actor_controlled_by_player[editor__tile_actor_idx(map, i, j)]
			= PLAYER_CNT_MAX - 1;
	break;
	case TILE_DOOR:
		map->tiles[i][j].type = TILE_CLONE2;
	break;
	case TILE_CLONE2:
		map->tiles[i][j].type = TILE_CLONE;
	break;
	}
}

static
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

					while (map->tiles[i][j].type != TILE_BLANK) {
						editor__rotate_tile_cw(map);
						history_push(&editor_player.history, ACTION_ROTATE_CW, 0);
						++change_cnt;
					}
					map->tiles[i][j].type = TILE_WALL;
					history_push(&editor_player.history, ACTION_ROTATE_CW, 0);
					++change_cnt;
				}
			break;
			case TILE_WALL:
				if (!editor__wall_needed_at_tile(map, i, j)) {
					u32 num_moves;
					editor__cursor_move_to(i, j, &num_moves);
					change_cnt += num_moves;

					while (map->tiles[i][j].type != TILE_BLANK) {
						editor__rotate_tile_ccw(map);
						history_push(&editor_player.history, ACTION_ROTATE_CCW, 0);
						++change_cnt;
					}
				}
			break;
			case TILE_HALL:
			case TILE_ACTOR:
			case TILE_CLONE:
			case TILE_CLONE2:
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
		history_push(&editor_player.history, ACTION_COUNT, change_cnt);
}

static
void editor__shrink_map(struct map *map)
{
	struct map map_play;
	v2i min;

	map_play.dim = editor__map_dim(map, &min);

	if (!v2i_equal(map_play.dim, g_v2i_zero)) {
		for (s32 i = 0; i < map_play.dim.y; ++i)
			for (s32 j = 0; j < map_play.dim.x; ++j)
				map_play.tiles[i][j] = map->tiles[i + min.y][j + min.x];

		map->dim = map_play.dim;
		memcpy(map->tiles, map_play.tiles, sizeof(map_play.tiles));
	} else {
		map->dim = g_v2i_zero;
	}
}

static
void editor__restore_map(struct map *map)
{
	editor__cleanup_map_border(map);
	editor__shrink_map(map);
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
		editor__cleanup_map_border(editor_map);

		{
			static const v2i max_dim = { MAP_DIM_MAX, MAP_DIM_MAX };
			v2i old_offset;
			const v2i dim = editor__map_dim(editor_map, &old_offset);
			const v2i new_offset = v2i_scale_inv(v2i_sub(max_dim, dim), 2);
			const v2i delta = v2i_sub(new_offset, old_offset);
			v2i_add_eq(&editor_cursor, delta);
		}

		editor__shrink_map(editor_map);

		*map_to_play = editor_map_idx;
	} else {
		*map_to_play = ~0;
	}

	gui_dim(gui, &screen.x, &screen.y);
	offset = v2i_scale_inv(v2i_sub(screen, v2i_scale(editor_map->dim, TILE_SIZE)), 2);

	if (gui_any_widget_has_focus(gui)) {
	} else if (key_mod(gui, KBM_CTRL)) {
		if (key_pressed(gui, KB_X)) {
			editor_map_cut = *editor_map;
			array_remove(*editor_maps, editor_map_idx);
			if (editor_map_idx == array_sz(*editor_maps)) {
				const struct map map_empty = { 0 };
				array_append(*editor_maps, map_empty);
			} else {
				editor_map = &(*editor_maps)[editor_map_idx];
			}
			editor__init_map(*editor_map);
			history_clear(&editor_player.history);
		} else if (key_pressed(gui, KB_C)) {
			editor_map_cut = *editor_map;
		} else if (   key_pressed(gui, KB_V)
		           && !v2i_equal(editor_map_cut.dim, g_v2i_zero)) {
			editor__restore_map(editor_map);
			array_insert(*editor_maps, editor_map_idx, editor_map_cut);
			editor_map = &(*editor_maps)[editor_map_idx];
			history_clear(&editor_player.history);
			editor_map_cut.dim = g_v2i_zero;
		}
	} else if (player_desires_action(&editor_player, ACTION_MOVE_UP, gui)) {
		editor_cursor.y = min(editor_cursor.y + 1, MAP_DIM_MAX - 1);
		history_push(&editor_player.history, ACTION_MOVE_UP, 0);
	} else if (player_desires_action(&editor_player, ACTION_MOVE_DOWN, gui)) {
		editor_cursor.y = max(editor_cursor.y - 1, 0);
		history_push(&editor_player.history, ACTION_MOVE_DOWN, 0);
	} else if (player_desires_action(&editor_player, ACTION_MOVE_LEFT, gui)) {
		editor_cursor.x = max(editor_cursor.x - 1, 0);
		history_push(&editor_player.history, ACTION_MOVE_LEFT, 0);
	} else if (player_desires_action(&editor_player, ACTION_MOVE_RIGHT, gui)) {
		editor_cursor.x = min(editor_cursor.x + 1, MAP_DIM_MAX - 1);
		history_push(&editor_player.history, ACTION_MOVE_RIGHT, 0);
	} else if (player_desires_action(&editor_player, ACTION_ROTATE_CW, gui)) {
		editor__rotate_tile_cw(editor_map);
		history_push(&editor_player.history, ACTION_ROTATE_CW, 0);
	} else if (player_desires_action(&editor_player, ACTION_ROTATE_CCW, gui)) {
		editor__rotate_tile_ccw(editor_map);
		history_push(&editor_player.history, ACTION_ROTATE_CCW, 0);
	} else if (player_desires_action(&editor_player, ACTION_UNDO, gui)) {
		enum action action;
		u32 num_clones;
		u32 remaining = 1;
		b32 count_moves = false;
		while (remaining && history_pop(&editor_player.history, &action, &num_clones)) {
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
	} else if (player_desires_action(&editor_player, ACTION_RESET, gui)) {
		editor__init_map(editor_map_orig);
		history_clear(&editor_player.history);
	} else if (key_pressed(gui, key_next)) {
		editor__restore_map(editor_map);
		if (editor_map_idx + 1 == array_sz(*editor_maps)) {
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
		history_clear(&editor_player.history);
		editor_map = &(*editor_maps)[editor_map_idx];
		editor__init_map((*editor_maps)[editor_map_idx]);
	} else if (key_pressed(gui, key_prev)) {
		editor__restore_map(editor_map);
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
		history_clear(&editor_player.history);
		editor_map = &(*editor_maps)[editor_map_idx];
		editor__init_map((*editor_maps)[editor_map_idx]);
	}

	gui_npt(gui, 2, screen.y - TILE_SIZE + 2, screen.x - 4, TILE_SIZE - 4,
	        editor_map->desc, MAP_TIP_MAX - 1, "description", 0);

	{
		u32 actor_idx = 0;
		for (s32 i = 0; i < editor_map->dim.y; ++i) {
			const s32 y = offset.y + i * TILE_SIZE;
			for (s32 j = 0; j < editor_map->dim.x; ++j) {
				const s32 x = offset.x + j * TILE_SIZE;
				const enum tile_type type = editor_map->tiles[i][j].type;
				const color_t color = { .r=0x32, .g=0x2f, .b=0x2f, .a=0xff };
				gui_rect(gui, x, y, TILE_SIZE, TILE_SIZE, g_tile_fills[type], color);
				if (type == TILE_ACTOR) {
					char buf[8];
					sprintf(buf, "%u", editor_map->actor_controlled_by_player[actor_idx] + 1);
					gui_txt(gui, x + TILE_SIZE / 2, y + TILE_SIZE / 2,
					        TILE_SIZE, buf, g_white, GUI_ALIGN_MIDCENTER);
					++actor_idx;
				}
			}
		}
	}

	gui_npt(gui, 2, 2, screen.x - 4, TILE_SIZE - 4,
	        editor_map->tip, MAP_TIP_MAX - 1, "tip", 0);

	gui_rect(gui, offset.x + editor_cursor.x * TILE_SIZE,
	         offset.y + editor_cursor.y * TILE_SIZE,
	         TILE_SIZE, TILE_SIZE, g_nocolor, g_white);
}
