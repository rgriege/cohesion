#include <time.h>
#include "config.h"
#define VIOLET_IMPLEMENTATION
#include "violet/all.h"
#include "key.h"
#include "action.h"
#include "types.h"
#include "constants.h"
#include "history.h"
#include "settings.h"
#include "disk.h"
#include "actor.h"
#include "player.h"
#include "level.h"

struct state
{
	struct actor actors[ACTOR_CNT_MAX];
	u32 num_actors;
};

struct stats
{
	u32 num_actors;
	u32 num_clones;
	u32 action_space;
	u32 num_solutions;
	u32 min_solution_steps;
};

static
v2i perp(v2i v, enum action action)
{
	switch (action) {
	case ACTION_ROTATE_CCW:
		return v2i_lperp(v);
	break;
	case ACTION_ROTATE_CW:
		return v2i_rperp(v);
	break;
	case ACTION_MOVE_UP:
	case ACTION_MOVE_DOWN:
	case ACTION_MOVE_LEFT:
	case ACTION_MOVE_RIGHT:
	case ACTION_UNDO:
	case ACTION_RESET:
	case ACTION_COUNT:
	break;
	}
	assert(false);
	return v;
}

static
void execute_action(enum action action, struct level *level, struct history *history)
{
	struct actor *actor;
	v2i disp;
	u32 num_clones_attached = 0;

	switch (action) {
	case ACTION_MOVE_UP:
	case ACTION_MOVE_DOWN:
	case ACTION_MOVE_LEFT:
	case ACTION_MOVE_RIGHT:
		disp = g_dir_vec[g_action_dir[action]];
		for (u32 i = 0; i < level->num_actors; ++i) {
			actor = &level->actors[i];
			v2i_add_eq(&actor->tile, disp);
			actor_entered_tile(actor, level, &num_clones_attached);
		}
	break;
	case ACTION_ROTATE_CCW:
	case ACTION_ROTATE_CW:
		for (u32 i = 0; i < level->num_actors; ++i) {
			actor = &level->actors[i];
			for (u32 j = 0; j < actor->num_clones; ++j)
				actor->clones[j] = perp(actor->clones[j], action);
			actor_entered_tile(actor, level, &num_clones_attached);
		}
	break;
	case ACTION_UNDO:
	case ACTION_RESET:
	case ACTION_COUNT:
		assert(false);
	break;
	}
	history_push(history, action, num_clones_attached);
}

static
void undo(struct level *level, struct history *history)
{
	for (u32 i = 0; i < level->num_actors; ++i) {
		struct actor *actor = &level->actors[level->num_actors - i - 1];
		enum action action;
		u32 num_clones;
		if (history_pop(history, &action, &num_clones)) {
			for (u32 j = 0; j < num_clones; ++j) {
				const v2i tile_relative = actor->clones[--actor->num_clones];
				const v2i tile_absolute = v2i_add(actor->tile, tile_relative);
				level->clones[level->num_clones++] = tile_absolute;
			}
			switch (action) {
			case ACTION_MOVE_UP:
				--actor->tile.y;
			break;
			case ACTION_MOVE_DOWN:
				++actor->tile.y;
			break;
			case ACTION_MOVE_LEFT:
				++actor->tile.x;
			break;
			case ACTION_MOVE_RIGHT:
				--actor->tile.x;
			break;
			case ACTION_ROTATE_CW:
				for (u32 j = 0; j < actor->num_clones; ++j)
					actor->clones[j] = v2i_lperp(actor->clones[j]);
			break;
			case ACTION_ROTATE_CCW:
				for (u32 j = 0; j < actor->num_clones; ++j)
					actor->clones[j] = v2i_rperp(actor->clones[j]);
			break;
			case ACTION_UNDO:
			case ACTION_RESET:
			case ACTION_COUNT:
				assert(false);
			break;
			}
		}
	}
}

static
b32 state_eq(const struct state *lhs, const struct state *rhs)
{
	assert(lhs->num_actors == rhs->num_actors);
	for (u32 i = 0; i < lhs->num_actors; ++i) {
		if (!v2i_equal(lhs->actors[i].tile, rhs->actors[i].tile))
			return false;
		if (lhs->actors[i].num_clones != rhs->actors[i].num_clones)
			return false;
		for (u32 j = 0; j < lhs->actors[i].num_clones; ++j)
			if (!v2i_equal(lhs->actors[i].clones[j], rhs->actors[i].clones[j]))
				return false;
	}
	return true;
}

static
b32 state_duplicated(array(struct state) states, struct state *state)
{
	array_foreach(states, struct state, s)
		if (state_eq(s, state))
			return true;
	return false;
}

/* assume 1 actor for now */
static
void recurse(struct level *level, u32 moves, array(struct state) *states,
             struct history *history, struct stats *stats)
{
	struct state state;

	if (moves > 30)
		return;

	for (u32 i = 0; i < ACTION_COUNT; ++i) {
		b32 possible = true;

		if (!(action_is_solo(i) && i != ACTION_UNDO))
			continue;

		for (u32 j = 0; j < level->num_actors; ++j)
			if (!actor_can_act(&level->actors[j], level, i))
				possible = false;
		if (!possible)
			continue;

		// printf("%*s%s\n", moves, "", action_to_string(i));
		execute_action(i, level, history);

		state.num_actors = level->num_actors;
		memcpy(state.actors, level->actors, sizeof(struct actor) * ACTOR_CNT_MAX);

		if (state_duplicated(*states, &state)) {
		} else if (level_complete(level)) {
			++stats->num_solutions;
			if (moves < stats->min_solution_steps)
				stats->min_solution_steps = moves;
		} else {
			array_append(*states, state);
			recurse(level, moves + 1, states, history, stats);
		}

		undo(level, history);
	}
}

void map_stats(const struct map *map, struct stats *stats)
{
	struct level level;
	struct player players[PLAYER_CNT_MAX];
	array(struct state) states;
	struct history history;

	level_init(&level, players, map);

	stats->num_actors = level.num_actors;
	stats->num_clones = level.num_clones;
	for (u32 i = 0; i < level.num_actors; ++i)
		stats->num_clones += level.actors[i].num_clones;
	stats->action_space = 0;
	stats->num_solutions = 0;
	stats->min_solution_steps = UINT_MAX;

	states = array_create();
	{
		struct state state;
		state.num_actors = level.num_actors;
		memcpy(state.actors, level.actors, sizeof(struct actor) * ACTOR_CNT_MAX);
		array_append(states, state);
	}

	history_clear(&history);

	recurse(&level, 0, &states, &history, stats);
	stats->action_space = array_sz(states);
	array_destroy(states);
}

int main(int argc, char *const argv[])
{
	array(struct map) maps;
	struct stats stats;
	int map = ~0;

	if (argc > 1)
		map = atoi(argv[1]) - 1;

	maps = array_create();
	if (!load_maps("maps.vson", &maps)) {
		fprintf(stderr, "failed to load maps\n");
		return 1;
	}

	if (array_empty(maps)) {
		fprintf(stderr, "no maps in file\n");
		return 1;
	}

	printf("%10s,%10s,%10s,%10s,%10s,%10s\n", "level", "actors", "clones", "space",
	       "solutions", "steps");
	if (map == ~0) {
		array_iterate(maps, i, n) {
			map_stats(&maps[i], &stats);
			printf("%10u,%10u,%10u,%10u,%10u,%10u\n", i + 1, stats.num_actors, stats.num_clones,
						 stats.action_space, stats.num_solutions, stats.min_solution_steps);
		}
	} else {
		map = clamp(0, map, array_sz(maps));
		map_stats(&maps[map], &stats);
		printf("%10u,%10u,%10u,%10u,%10u,%10u\n", map + 1, stats.num_actors, stats.num_clones,
		       stats.action_space, stats.num_solutions, stats.min_solution_steps);
	}
	return 0;
}
