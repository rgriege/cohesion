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
	u32 from;
	enum action in_action;
	u32 cost;
	b32 complete;
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
				actor->clones[j].pos = perp(actor->clones[j].pos, action);
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
				const b32 required = actor->clones[actor->num_clones-1].required;
				const v2i tile_relative = actor->clones[actor->num_clones-1].pos;
				const v2i tile_absolute = v2i_add(actor->tile, tile_relative);
				level->clones[level->num_clones].pos = tile_absolute;
				level->clones[level->num_clones].required = required;
				--actor->num_clones;
				++level->num_clones;
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
					actor->clones[j].pos = v2i_lperp(actor->clones[j].pos);
			break;
			case ACTION_ROTATE_CCW:
				for (u32 j = 0; j < actor->num_clones; ++j)
					actor->clones[j].pos = v2i_rperp(actor->clones[j].pos);
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
		for (u32 j = 0; j < lhs->actors[i].num_clones; ++j) {
			u32 k;
			for (k = 0; k < rhs->actors[i].num_clones; ++k)
				if (   v2i_equal(lhs->actors[i].clones[j].pos, rhs->actors[i].clones[k].pos)
				    && lhs->actors[i].clones[j].required == rhs->actors[i].clones[k].required)
					break;
			if (k == rhs->actors[i].num_clones)
				return false;
		}
	}
	return true;
}

static
b32 state_duplicated(array(struct state) states, struct state *state, u32 *idx)
{
	array_iterate(states, i, n) {
		if (state_eq(&states[i], state)) {
			*idx = i;
			return true;
		}
	}
	return false;
}

/* assume 1 actor for now */
static
void recurse(struct level *level, array(struct state) *states,
             struct history *history, struct stats *stats)
{
	struct state state = {
		.cost = array_last(*states).cost + 1,
		.from = array_sz(*states) - 1,
	};

	if (state.cost > 30)
		return;

	for (u32 i = 0; i < ACTION_COUNT; ++i) {
		b32 possible = true;
		u32 dup_idx;

		if (!(action_is_solo(i) && i != ACTION_UNDO))
			continue;

		for (u32 j = 0; j < level->num_actors; ++j)
			if (!actor_can_act(&level->actors[j], level, i))
				possible = false;
		if (!possible)
			continue;

		execute_action(i, level, history);

		state.num_actors = level->num_actors;
		memcpy(state.actors, level->actors, sizeof(struct actor) * ACTOR_CNT_MAX);

		if (state_duplicated(*states, &state, &dup_idx)) {
			if ((*states)[dup_idx].cost > state.cost) {
				(*states)[dup_idx].cost = state.cost;
				(*states)[dup_idx].from = state.from;
				(*states)[dup_idx].in_action = i;
			}
		} else if (level_complete(level)) {
			state.in_action = i;
			state.complete = true;
			array_append(*states, state);
		} else {
			state.in_action = i;
			state.complete = false;
			array_append(*states, state);
			recurse(level, states, history, stats);
		}

		undo(level, history);
	}
}

void map_stats(const struct map *map, struct stats *stats, b32 detail)
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
		struct state state = { .cost = 0, .from = UINT_MAX, .in_action = ACTION_COUNT, };
		state.num_actors = level.num_actors;
		memcpy(state.actors, level.actors, sizeof(struct actor) * ACTOR_CNT_MAX);
		array_append(states, state);
	}

	history_clear(&history);

	recurse(&level, &states, &history, stats);
	stats->action_space = array_sz(states);

	{
		b32 updated = true;
		while (updated) {
			updated = false;
			array_foreach(states, struct state, state) {
				if (state->from == UINT_MAX)
					continue;
				if (states[state->from].cost + 1 < state->cost) {
					state->cost = states[state->from].cost + 1;
					updated = true;
				}
			}
		}
	}
	array_foreach(states, struct state, state) {
		if (state->complete) {
			++stats->num_solutions;
			stats->min_solution_steps = min(stats->min_solution_steps, state->cost);
		}
	}

	if (detail) {
		array(struct state*) solution = array_create();
		array_foreach(states, struct state, state) {
			if (state->complete) {
				array_clear(solution);
				struct state *s = state;
				printf("%u: ", s->cost);
				while (s) {
					array_append(solution, s);
					s = s->from != UINT_MAX ? &states[s->from] : NULL;
				}
				for (u32 i = array_sz(solution) - 2; i > 0; --i)
					printf("%s, ", action_to_string(solution[i]->in_action));
				printf("%s", action_to_string(solution[0]->in_action));
				printf("\n");
			}
		}
		array_destroy(solution);
	}

	array_destroy(states);
}

int main(int argc, char *const argv[])
{
	array(struct map) maps;
	struct stats stats;
	const char *fname = "maps.vson";
	b32 detail = false;
	int map = ~0;

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-v") == 0)
			detail = true;
		else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc)
			fname = argv[++i];
		else if (strcmp(argv[i], "--level") == 0 && i + 1 < argc)
			map = atoi(argv[++i]) - 1;
	}

	maps = array_create();
	if (!load_maps(fname, &maps)) {
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
			map_stats(&maps[i], &stats, false);
			printf("%10u,%10u,%10u,%10u,%10u,%10u\n", i + 1, stats.num_actors, stats.num_clones,
						 stats.action_space, stats.num_solutions, stats.min_solution_steps);
		}
	} else {
		map = clamp(0, map, array_sz(maps));
		map_stats(&maps[map], &stats, detail);
		printf("%10u,%10u,%10u,%10u,%10u,%10u\n", map + 1, stats.num_actors, stats.num_clones,
		       stats.action_space, stats.num_solutions, stats.min_solution_steps);
	}
	return 0;
}
