#include "config.h"
#include "violet/all.h"
#include "action.h"
#include "types.h"
#include "actor.h"
#include "player.h"
#include "level.h"

void level_init(struct level *level, struct player players[], const struct map *map)
{
	struct actor *actor;
	u32 player_idx;
	struct player *player;

	for (u32 i = 0; i < PLAYER_CNT_MAX; ++i)
		player_init(&players[i], i);

	level->map = *map;
	level->num_actors = 0;
	level->num_clones = 0;
	for (s32 i = 0; i < map->dim.y; ++i) {
		for (s32 j = 0; j < map->dim.x; ++j) {
			level->map.tiles[i][j].t = 0.f;
#ifdef SHOW_TRAVELLED
			level->map.tiles[i][j].travelled = false;
#endif
			switch(map->tiles[i][j].type) {
			case TILE_BLANK:
			case TILE_HALL:
			case TILE_WALL:
			case TILE_DOOR:
			break;
			case TILE_ACTOR:
				actor = &level->actors[level->num_actors];
				player_idx = map->actor_controlled_by_player[level->num_actors];
				player = &players[player_idx];

				actor_init(actor, player_idx, j, i, level);
				++level->num_actors;

				player->actors[player->num_actors] = actor;
				++player->num_actors;

				level->map.tiles[i][j].type = TILE_HALL;
#ifdef SHOW_TRAVELLED
				level->map.tiles[i][j].travelled = true;
#endif
			break;
			case TILE_CLONE:
			case TILE_CLONE2:
				level->clones[level->num_clones].pos = (v2i){ .x = j, .y = i };
				level->clones[level->num_clones].required = map->tiles[i][j].type == TILE_CLONE2;
				++level->num_clones;
				level->map.tiles[i][j].type = TILE_HALL;
#ifdef SHOW_TRAVELLED
				level->map.tiles[i][j].travelled = true;
#endif
			break;
			}
		}
	}
	for (u32 i = 0; i < level->num_actors; ++i) {
		u32 num_clones_attached;
		actor_entered_tile(&level->actors[i], level, &num_clones_attached);
	}
	level->complete = false;
}

b32 level_complete(const struct level *level)
{
	for (u32 i = 0; i < level->num_actors; ++i) {
		const struct actor *actor = &level->actors[i];
		if (   level->map.tiles[actor->tile.y][actor->tile.x].type != TILE_DOOR
		    || actor->dir != DIR_NONE)
			return false;
	}
	for (u32 i = 0; i < level->num_clones; ++i)
		if (level->clones[i].required)
			return false;
	return true;
}
