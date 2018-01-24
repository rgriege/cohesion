#include "config.h"
#include "violet/all.h"
#include "action.h"
#include "types.h"
#include "constants.h"
#include "actor.h"

void actor_init(struct actor *actor, u32 player, s32 x, s32 y, struct level *level)
{
	actor->player = player;
	actor->tile = (v2i){ .x = x, .y = y };
	actor->pos = v2i_to_v2f(v2i_scale(actor->tile, TILE_SIZE));
	actor->dir = DIR_NONE;
	actor->facing = DIR_DOWN;
	actor->anim_milli = 0;
	actor->num_clones = 0;
	actor_entered_tile(actor, level, NULL);
}

static
s32 manhattan_dist(v2i v0, v2i v1)
{
	return abs(v0.x - v1.x) + abs(v0.y - v1.y);
}

void actor_entered_tile(struct actor *actor, struct level *level,
                        u32 *num_clones_attached)
{
	const v2i tile = actor->tile;
	const u32 num_clones_attached_start = actor->num_clones;
#ifdef SHOW_TRAVELLED
	level->map.tiles[tile.y][tile.x].travelled = true;
	for (u32 i = 0; i < actor->num_clones; ++i) {
		const v2i actor_clone = v2i_add(tile, actor->clones[i].pos);
		level->map.tiles[actor_clone.y][actor_clone.x].travelled = true;
	}
#endif

	for (u32 j = 0; j < level->num_clones; ) {
		if (manhattan_dist(level->clones[j].pos, tile) == 1) {
			actor->clones[actor->num_clones].pos = v2i_sub(level->clones[j].pos, tile);
			actor->clones[actor->num_clones].required = level->clones[j].required;
			++actor->num_clones;
			level->clones[j] = level->clones[--level->num_clones];
		} else {
			++j;
		}
	}

clones:
	for (u32 i = 0; i < actor->num_clones; ++i) {
		const v2i actor_clone = v2i_add(tile, actor->clones[i].pos);
		for (u32 j = 0; j < level->num_clones; ) {
			if (manhattan_dist(level->clones[j].pos, actor_clone) == 1) {
				actor->clones[actor->num_clones].pos = v2i_sub(level->clones[j].pos, tile);
				actor->clones[actor->num_clones].required = level->clones[j].required;
				++actor->num_clones;
				level->clones[j] = level->clones[--level->num_clones];
				goto clones;
			} else {
				++j;
			}
		}
	}
	if (num_clones_attached)
		*num_clones_attached = actor->num_clones - num_clones_attached_start;
}

static
b32 tile_occupied(const struct level *level, v2i tile,
                  const struct actor *excluded_actor)
{
	if (tile.x < 0 || tile.x >= level->map.dim.x)
		return true;
	if (tile.y < 0 || tile.y >= level->map.dim.y)
		return true;
	if (   level->map.tiles[tile.y][tile.x].type != TILE_HALL
	    && level->map.tiles[tile.y][tile.x].type != TILE_DOOR)
		return true;
	for (u32 i = 0; i < level->num_clones; ++i)
		if (v2i_equal(level->clones[i].pos, tile))
			return true;
	for (u32 i = 0; i < level->num_actors; ++i) {
		const struct actor *actor = &level->actors[i];
		if (actor->player == excluded_actor->player)
			continue;
		if (v2i_equal(actor->tile, tile))
			return true;
		for (u32 j = 0; j < actor->num_clones; ++j)
			if (v2i_equal(v2i_add(actor->tile, actor->clones[j].pos), tile))
				return true;
	}
	return false;
}

static
b32 actor_can_move(const struct actor *actor, const struct level *level,
                   v2i offset)
{
	const v2i tile = v2i_add(actor->tile, offset);
	if (tile_occupied(level, tile, actor))
		return false;
	for (u32 i = 0; i < actor->num_clones; ++i) {
		const v2i clone_tile = v2i_add(tile, actor->clones[i].pos);
		if (tile_occupied(level, clone_tile, actor))
			return false;
	}
	return true;
}

static
b32 actor_can_rotate(const struct actor *actor, const struct level *level,
                     b32 clockwise)
{
	v2i(*perp)(v2i) = clockwise ? v2i_rperp : v2i_lperp;
	for (u32 i = 0; i < actor->num_clones; ++i) {
		const v2i clone_tile = v2i_add(actor->tile, perp(actor->clones[i].pos));
		if (tile_occupied(level, clone_tile, actor))
			return false;
	}
	return true;
}

b32 actor_can_act(const struct actor *actor, const struct level *level,
                  enum action action)
{
	switch (action) {
	case ACTION_MOVE_UP:
		if (!actor_can_move(actor, level, g_v2i_up))
			return false;
	break;
	case ACTION_MOVE_DOWN:
		if (!actor_can_move(actor, level, g_v2i_down))
			return false;
	break;
	case ACTION_MOVE_LEFT:
		if (!actor_can_move(actor, level, g_v2i_left))
			return false;
	break;
	case ACTION_MOVE_RIGHT:
		if (!actor_can_move(actor, level, g_v2i_right))
			return false;
	break;
	case ACTION_ROTATE_CCW:
		if (!actor_can_rotate(actor, level, false))
			return false;
	break;
	case ACTION_ROTATE_CW:
		if (!actor_can_rotate(actor, level, true))
			return false;
	break;
	case ACTION_UNDO:
	case ACTION_RESET:
	break;
	case ACTION_COUNT:
		assert(false);
	}
	return true;
}

b32 actor_can_undo(const struct actor *actor, const struct level *level,
                   enum action action, u32 num_clones_acquired)
{
	const u32 num_clones = actor->num_clones - num_clones_acquired;
	switch (action) {
	case ACTION_MOVE_UP:
		if (tile_occupied(level, v2i_add(actor->tile, g_v2i_down), actor))
			return false;
		for (u32 j = 0; j < num_clones; ++j) {
			const v2i clone_tile = v2i_add(actor->tile, actor->clones[j].pos);
			if (tile_occupied(level, v2i_add(clone_tile, g_v2i_down), actor))
				return false;
		}
	break;
	case ACTION_MOVE_DOWN:
		if (tile_occupied(level, v2i_add(actor->tile, g_v2i_up), actor))
			return false;
		for (u32 j = 0; j < num_clones; ++j) {
			const v2i clone_tile = v2i_add(actor->tile, actor->clones[j].pos);
			if (tile_occupied(level, v2i_add(clone_tile, g_v2i_up), actor))
				return false;
		}
	break;
	case ACTION_MOVE_LEFT:
		if (tile_occupied(level, v2i_add(actor->tile, g_v2i_right), actor))
			return false;
		for (u32 j = 0; j < num_clones; ++j) {
			const v2i clone_tile = v2i_add(actor->tile, actor->clones[j].pos);
			if (tile_occupied(level, v2i_add(clone_tile, g_v2i_right), actor))
				return false;
		}
	break;
	case ACTION_MOVE_RIGHT:
		if (tile_occupied(level, v2i_add(actor->tile, g_v2i_left), actor))
			return false;
		for (u32 j = 0; j < num_clones; ++j) {
			const v2i clone_tile = v2i_add(actor->tile, actor->clones[j].pos);
			if (tile_occupied(level, v2i_add(clone_tile, g_v2i_left), actor))
				return false;
		}
	break;
	case ACTION_ROTATE_CW:
		for (u32 j = 0; j < num_clones; ++j) {
			const v2i clone_tile = v2i_add(actor->tile, v2i_lperp(actor->clones[j].pos));
			if (tile_occupied(level, clone_tile, actor))
				return false;
		}
	break;
	case ACTION_ROTATE_CCW:
		for (u32 j = 0; j < num_clones; ++j) {
			const v2i clone_tile = v2i_add(actor->tile, v2i_rperp(actor->clones[j].pos));
			if (tile_occupied(level, clone_tile, actor))
				return false;
		}
	break;
	case ACTION_UNDO:
	case ACTION_RESET:
	case ACTION_COUNT:
		assert(false);
	break;
	}
	return true;
}

