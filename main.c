#include <time.h>

#define VIOLET_IMPLEMENTATION
#include "violet/all.h"

#include <SDL_mixer.h>
#include "audio.h"

#define MAP_DIM_MAX 16
#define MAP_DESC_MAX 64
#define TILE_SIZE 20
#define WALK_SPEED 80
#define CLONE_CNT_MAX 32
#define FPS_CAP 30
#define ALLOW_ROTATION
// #define SHOW_TRAVELLED
#define LEVEL_COMPLETE_EFFECT_DURATION_MILLI 1000
#define ROTATION_EFFECT_DURATION_MILLI 250

enum tile_type {
	TILE_BLANK,
	TILE_WALL,
	TILE_HALL,
	TILE_PLAYER,
	TILE_CLONE,
	TILE_DOOR,
};

struct tile {
	enum tile_type type;
#ifdef SHOW_TRAVELLED
	b32 travelled;
#endif
};

struct map {
	char desc[MAP_DESC_MAX];
	v2i dim;
	struct tile tiles[MAP_DIM_MAX][MAP_DIM_MAX];
};

struct level {
	struct map map;
	v2i player_start;
	v2i clones[CLONE_CNT_MAX];
	u32 num_clones;
	b32 complete;
};

enum dir {
	DIR_NONE,
	DIR_UP,
	DIR_DOWN,
	DIR_LEFT,
	DIR_RIGHT,
};

struct player {
	v2i tile;
	v2f pos;
	enum dir dir;
	v2i clones[CLONE_CNT_MAX];
	u32 num_clones;
};

struct effect {
	v2i pos;
	color_t color;
	r32 t;
	u32 duration;
};

const color_t g_tile_fills[] = {
	gi_nocolor,
	gi_black,
	gi_grey128,
	gi_orange,
	gi_lightblue,
	gi_yellow,
};

static
b32 load_maps(struct map **maps)
{
	b32 success = false;
	u32 n;
	FILE *fp;

	fp = fopen("maps.vson", "r");
	if (!fp)
		return false;

	if (!vson_read_u32(fp, "maps", &n))
		goto out;

	for (u32 i = 0; i < n; ++i) {
		struct map map;
		char row[MAP_DIM_MAX + 1];
		if (!vson_read_str(fp, "desc", map.desc, MAP_DESC_MAX))
			goto out;
		if (!vson_read_s32(fp, "width", &map.dim.x))
			goto out;
		if (!vson_read_s32(fp, "height", &map.dim.y))
			goto out;
		for (s32 i = 0; i < map.dim.y; ++i) {
			fgets(row, sizeof(row), fp);
			for (s32 j = 0; j < map.dim.x; ++j)
				map.tiles[map.dim.y - i - 1][j].type = row[j] - '0';
		}
		array_append(*maps, map);
	}
	success = true;

out:
	if (!success)
		array_clear(*maps);
	return success;
}

static
void level_init(struct level *level, const struct map *map)
{
	level->map = *map;
	level->num_clones = 0;
	for (s32 i = 0; i < map->dim.y; ++i) {
		for (s32 j = 0; j < map->dim.x; ++j) {
#ifdef SHOW_TRAVELLED
			level->map.tiles[i][j].travelled = false;
#endif
			switch(map->tiles[i][j].type) {
			case TILE_BLANK:
			case TILE_HALL:
			case TILE_WALL:
			case TILE_DOOR:
			break;
			case TILE_PLAYER:
				level->player_start = (v2i){ .x = j, .y = i };
				level->map.tiles[i][j].type = TILE_HALL;
#ifdef SHOW_TRAVELLED
				level->map.tiles[i][j].travelled = true;
#endif
			break;
			case TILE_CLONE:
				level->clones[level->num_clones++] = (v2i){ .x = j, .y = i };
				level->map.tiles[i][j].type = TILE_HALL;
#ifdef SHOW_TRAVELLED
				level->map.tiles[i][j].travelled = true;
#endif
			break;
			}
		}
	}
	level->complete = false;
}

static
void player_init(struct player *player, const struct level *level)
{
	player->tile = level->player_start;
	player->pos = v2i_to_v2f(v2i_scale(player->tile, TILE_SIZE));
	player->dir = DIR_NONE;
	player->num_clones = 0;
}

static
b32 tile_walkable(const struct level *level, s32 x, s32 y)
{
	if (x < 0 || x >= level->map.dim.x)
		return false;
	if (y < 0 || y >= level->map.dim.y)
		return false;
	if (   level->map.tiles[y][x].type != TILE_HALL
	    && level->map.tiles[y][x].type != TILE_DOOR)
		return false;
	for (u32 i = 0; i < level->num_clones; ++i)
		if (level->clones[i].x == x && level->clones[i].y == y)
			return false;
	return true;
}

static
b32 tile_walkablev(const struct level *level, v2i tile)
{
	return tile_walkable(level, tile.x, tile.y);
}

static
b32 tile_walkable_for_player(const struct level *level, s32 x, s32 y,
                             const struct player *player)
{
	if (!tile_walkable(level, x, y))
		return false;
	for (u32 i = 0; i < player->num_clones; ++i)
		if (!tile_walkable(level, x + player->clones[i].x, y + player->clones[i].y))
			return false;
	return true;
}

static
void render_player(gui_t *gui, v2i offset, v2i pos, color_t color)
{
	gui_rect(gui, offset.x + 2 + pos.x, offset.y + 2 + pos.y, TILE_SIZE - 4, TILE_SIZE - 4, color, g_nocolor);
}

static
s32 manhattan_dist(v2i v0, v2i v1)
{
	return abs(v0.x - v1.x) + abs(v0.y - v1.y);
}

static
void player_entered_tile(struct level *level, v2i tile, struct player *player)
{
#ifdef SHOW_TRAVELLED
	level->map.tiles[tile.y][tile.x].travelled = true;
	for (u32 i = 0; i < player->num_clones; ++i) {
		const v2i player_clone = v2i_add(tile, player->clones[i]);
		level->map.tiles[player_clone.y][player_clone.x].travelled = true;
	}
#endif

	for (u32 j = 0; j < level->num_clones; ) {
		if (manhattan_dist(level->clones[j], tile) == 1) {
			player->clones[player->num_clones++] = v2i_sub(level->clones[j], tile);
			level->clones[j] = level->clones[--level->num_clones];
		} else {
			++j;
		}
	}

clones:
	for (u32 i = 0; i < player->num_clones; ++i) {
		const v2i player_clone = v2i_add(tile, player->clones[i]);
		for (u32 j = 0; j < level->num_clones; ) {
			if (manhattan_dist(level->clones[j], player_clone) == 1) {
				player->clones[player->num_clones++] = v2i_sub(level->clones[j], tile);
				level->clones[j] = level->clones[--level->num_clones];
				goto clones;
			} else {
				++j;
			}
		}
	}
}

static
void disolve_effect_add(array(struct effect) *effects, v2i offset, v2i tile, color_t fill, u32 duration)
{
	static const v2i half_time = { .x = TILE_SIZE / 2, .y = TILE_SIZE / 2 };
	const struct effect fx = {
		.pos = v2i_add(v2i_add(offset, v2i_scale(tile, TILE_SIZE)), half_time),
		.color = fill,
		.t = 0.f,
		.duration = duration,
	};
	array_append(*effects, fx);
}

static
void background_generate(array(struct effect) *effects, v2i screen)
{
	array_clear(*effects);

	for (u32 i = 0; i < 3; ++i) {
		for (u32 j = 0; j < 3; ++j) {
			const struct effect fx = {
				.pos = {
					.x = j * screen.x / 3 + rand() % screen.x / 3,
					.y = i * screen.y / 3 + rand() % screen.y / 3
				},
				.color = g_tile_fills[rand() % 5 + 1],
				.t = (random() % 100) / 100.f,
				.duration = 2000 + rand() % 1000,
			};
			array_append(*effects, fx);
		}
	}
}

int main(int argc, char *const argv[]) {
	gui_t *gui;
	b32 quit = false;
	u32 level_idx = 0;
	struct level level;
	struct player player;
	u32 frame_milli = 0;
	struct sound sound_error, sound_swipe, sound_success;
	array(struct map) maps;
	array(struct effect) bg_effects;
	array(struct effect) disolve_effects;
	v2i screen, offset;

	log_add_std(LOG_STDOUT);

	srand(time(NULL));
	
	gui = gui_create(0, 0, (MAP_DIM_MAX + 2) * TILE_SIZE, (MAP_DIM_MAX + 2) * TILE_SIZE + 40, "ldjam", WINDOW_CENTERED);
	if (!gui)
		return 1;
	gui_dim(gui, &screen.x, &screen.y);

	if (!audio_init())
		goto err_audio;

	if (!sound_init(&sound_error, "error.aiff"))
		goto err_sound_error;

	assert(sound_init(&sound_swipe, "swipe.aiff"));

	if (!sound_init(&sound_success, "success.aiff"))
		goto err_sound_success;

	maps = array_create();
	if (!load_maps(&maps) || array_empty(maps))
		goto err_maps_load;

	level_init(&level, &maps[level_idx]);
	player_init(&player, &level);

	bg_effects = array_create();
	background_generate(&bg_effects, screen);

	disolve_effects = array_create();

	while (!quit && gui_begin_frame(gui)) {
		gui_dim(gui, &screen.x, &screen.y);
		offset = v2i_scale_inv(v2i_sub(screen, v2i_scale(level.map.dim, TILE_SIZE)), 2);

#ifdef DEBUG
		if (key_pressed(gui, KB_G))
			background_generate(&bg_effects, screen);
#endif // DEBUG

		for (u32 i = 0; i < array_sz(bg_effects); ) {
			struct effect *fx = &bg_effects[i];
      fx->t += (r32)frame_milli / fx->duration;
			if (fx->t <= 1.f) {
				const u32 sz = TILE_SIZE;
				color_t fill = fx->color;
				fill.a = 48 * fx->t + 16;
				gui_rect(gui, fx->pos.x - sz / 2, fx->pos.y - sz / 2, sz, sz, fill, g_nocolor);
				++i;
			} else if (fx->t <= 2.f) {
				const u32 sz = TILE_SIZE;
				color_t fill = fx->color;
				fill.a = 48 * (2.f - fx->t) + 16;
				gui_rect(gui, fx->pos.x - sz / 2, fx->pos.y - sz / 2, sz, sz, fill, g_nocolor);
				++i;
			} else {
				fx->t = 0.f;
			}
		}

		gui_txt(gui, offset.x + level.map.dim.x * TILE_SIZE / 2, offset.y - 20, 14, level.map.desc, g_white, GUI_ALIGN_CENTER);

		if (!level.complete) {
			for (s32 i = 0; i < level.map.dim.y; ++i) {
				const s32 y = offset.y + i * TILE_SIZE;
				for (s32 j = 0; j < level.map.dim.x; ++j) {
					const s32 x = offset.x + j * TILE_SIZE;
					const  enum tile_type type = level.map.tiles[i][j].type;
					switch (type) {
					case TILE_BLANK:
					break;
					case TILE_HALL:
					case TILE_PLAYER:
					case TILE_CLONE:
						gui_rect(gui, x, y, TILE_SIZE, TILE_SIZE, g_tile_fills[TILE_HALL], g_tile_fills[TILE_WALL]);
					break;
					case TILE_WALL:
						gui_rect(gui, x, y, TILE_SIZE, TILE_SIZE, g_tile_fills[TILE_WALL], g_tile_fills[TILE_HALL]);
					break;
					case TILE_DOOR:
						gui_rect(gui, x, y, TILE_SIZE, TILE_SIZE, g_tile_fills[TILE_DOOR], g_tile_fills[TILE_WALL]);
					break;
					}
#ifdef SHOW_TRAVELLED
					if (level.map.tiles[i][j].travelled)
						gui_circ(gui, x + TILE_SIZE / 2, y + TILE_SIZE / 2, TILE_SIZE / 4, g_red, g_nocolor);
#endif
				}
			}
		}

		for (u32 i = 0; i < array_sz(disolve_effects); ) {
			struct effect *fx = &disolve_effects[i];
      fx->t += (r32)frame_milli / fx->duration;
			if (fx->t <= 1.f) {
#ifndef SHOW_TRAVELLED
				const u32 sz = (TILE_SIZE - 4) * (1.f - fx->t);
				color_t fill = fx->color;
				fill.a = 255 * (1.f - fx->t);
				gui_rect(gui, fx->pos.x - sz / 2, fx->pos.y - sz / 2, sz, sz, fill, g_nocolor);
#endif // SHOW_TRAVELLED
				++i;
			} else {
				array_remove_fast(disolve_effects, i);
			}
		}

		if (!level.complete)
			for (u32 i = 0; i < level.num_clones; ++i)
				render_player(gui, offset, v2i_scale(level.clones[i], TILE_SIZE), g_tile_fills[TILE_CLONE]);

		switch (player.dir) {
		case DIR_NONE:
			if (   key_down(gui, KB_W)
			    && tile_walkable_for_player(&level, player.tile.x, player.tile.y + 1, &player)) {
				player.dir = DIR_UP;
				++player.tile.y;
			} else if (   key_down(gui, KB_S)
			           && tile_walkable_for_player(&level, player.tile.x, player.tile.y - 1, &player)) {
				player.dir = DIR_DOWN;
				--player.tile.y;
			} else if (   key_down(gui, KB_A)
			           && tile_walkable_for_player(&level, player.tile.x - 1, player.tile.y, &player)) {
				player.dir = DIR_LEFT;
				--player.tile.x;
			} else if (   key_down(gui, KB_D)
			           && tile_walkable_for_player(&level, player.tile.x + 1, player.tile.y, &player)) {
				player.dir = DIR_RIGHT;
				++player.tile.x;
#ifdef ALLOW_ROTATION
			} else if (key_pressed(gui, KB_Q)) {
				/* ccw */
				b32 can_rotate = true;
				for (u32 i = 0; i < player.num_clones; ++i)
					if (!tile_walkablev(&level, v2i_add(player.tile, v2i_lperp(player.clones[i]))))
						can_rotate = false;
				if (can_rotate) {
					for (u32 i = 0; i < player.num_clones; ++i) {
						disolve_effect_add(&disolve_effects, offset, v2i_add(player.tile, player.clones[i]), g_tile_fills[TILE_CLONE], ROTATION_EFFECT_DURATION_MILLI);
						player.clones[i] = v2i_lperp(player.clones[i]);
					}
					player_entered_tile(&level, player.tile, &player);
					sound_play(&sound_swipe);
				} else {
					sound_play(&sound_error);
				}
			} else if (key_pressed(gui, KB_E)) {
				/* cw */
				b32 can_rotate = true;
				for (u32 i = 0; i < player.num_clones; ++i)
					if (!tile_walkablev(&level, v2i_add(player.tile, v2i_rperp(player.clones[i]))))
						can_rotate = false;
				if (can_rotate) {
					for (u32 i = 0; i < player.num_clones; ++i) {
						disolve_effect_add(&disolve_effects, offset, v2i_add(player.tile, player.clones[i]), g_tile_fills[TILE_CLONE], ROTATION_EFFECT_DURATION_MILLI);
						player.clones[i] = v2i_rperp(player.clones[i]);
					}
					player_entered_tile(&level, player.tile, &player);
					sound_play(&sound_swipe);
				} else {
					sound_play(&sound_error);
				}
#endif // ALLOW_ROTATION
			}
		break;
		case DIR_UP:
			player.pos.y += WALK_SPEED * frame_milli / 1000.f;
			if ((s32)player.pos.y >= player.tile.y * TILE_SIZE) {
				player_entered_tile(&level, player.tile, &player);
				if (   key_down(gui, KB_W)
				    && tile_walkable_for_player(&level, player.tile.x, player.tile.y + 1, &player)) {
					++player.tile.y;
				} else {
					player.pos.y = player.tile.y * TILE_SIZE;
					player.dir = DIR_NONE;
				}
			}
		break;
		case DIR_DOWN:
			player.pos.y -= WALK_SPEED * frame_milli / 1000.f;
			if ((s32)player.pos.y <= player.tile.y * TILE_SIZE) {
				player_entered_tile(&level, player.tile, &player);
				if (   key_down(gui, KB_S)
				    && tile_walkable_for_player(&level, player.tile.x, player.tile.y - 1, &player)) {
					--player.tile.y;
				} else {
					player.pos.y = player.tile.y * TILE_SIZE;
					player.dir = DIR_NONE;
				}
			}
		break;
		case DIR_LEFT:
			player.pos.x -= WALK_SPEED * frame_milli / 1000.f;
			if ((s32)player.pos.x <= player.tile.x * TILE_SIZE) {
				player_entered_tile(&level, player.tile, &player);
				if (   key_down(gui, KB_A)
				    && tile_walkable_for_player(&level, player.tile.x - 1, player.tile.y, &player)) {
					--player.tile.x;
				} else {
					player.pos.x = player.tile.x * TILE_SIZE;
					player.dir = DIR_NONE;
				}
			}
		break;
		case DIR_RIGHT:
			player.pos.x += WALK_SPEED * frame_milli / 1000.f;
			if ((s32)player.pos.x >= player.tile.x * TILE_SIZE) {
				player_entered_tile(&level, player.tile, &player);
				if (   key_down(gui, KB_D)
				    && tile_walkable_for_player(&level, player.tile.x + 1, player.tile.y, &player)) {
					++player.tile.x;
				} else {
					player.pos.x = player.tile.x * TILE_SIZE;
					player.dir = DIR_NONE;
				}
			}
		break;
		}

		if (!level.complete) {
			render_player(gui, offset, v2f_to_v2i(player.pos), g_tile_fills[TILE_PLAYER]);
			for (u32 i = 0; i < player.num_clones; ++i)
				render_player(gui, offset, v2i_add(v2f_to_v2i(player.pos), v2i_scale(player.clones[i], TILE_SIZE)), g_tile_fills[TILE_CLONE]);
		}

		if (   !level.complete
		    && level.map.tiles[player.tile.y][player.tile.x].type == TILE_DOOR
		    && player.dir == DIR_NONE) {
			for (s32 i = 0; i < level.map.dim.y; ++i) {
				for (s32 j = 0; j < level.map.dim.x; ++j) {
					const v2i tile = { .x = j, .y = i };
					disolve_effect_add(&disolve_effects, offset, tile, g_tile_fills[level.map.tiles[i][j].type], LEVEL_COMPLETE_EFFECT_DURATION_MILLI);
				}
			}
			disolve_effect_add(&disolve_effects, offset, player.tile, g_tile_fills[TILE_PLAYER], LEVEL_COMPLETE_EFFECT_DURATION_MILLI);
			for (u32 i = 0; i < player.num_clones; ++i)
				disolve_effect_add(&disolve_effects, offset, v2i_add(player.tile, player.clones[i]), g_tile_fills[TILE_CLONE], LEVEL_COMPLETE_EFFECT_DURATION_MILLI);
			sound_play(&sound_success);
			level.complete = true;
		}

		if (level.complete && array_empty(disolve_effects)) {
			level_idx = (level_idx + 1) % array_sz(maps);
			level_init(&level, &maps[level_idx]);
			player_init(&player, &level);
			background_generate(&bg_effects, screen);
		}

		if (key_pressed(gui, KB_N)) {
			level_idx = (level_idx + 1) % array_sz(maps);
			level_init(&level, &maps[level_idx]);
			player_init(&player, &level);
		} else if (key_pressed(gui, KB_P)) {
			level_idx = (level_idx + array_sz(maps) - 1) % array_sz(maps);
			level_init(&level, &maps[level_idx]);
			player_init(&player, &level);
		} else if (key_pressed(gui, KB_R)) {
			if (key_mod(gui, KBM_CTRL)) {
				const struct map current_map = maps[level_idx];
				array_clear(maps);
				if (!load_maps(&maps) || array_empty(maps)) {
					array_append(maps, current_map);
					level_idx = 0;
				}
			}
			level_init(&level, &maps[level_idx]);
			player_init(&player, &level);
		}

		quit = key_down(gui, KB_ESCAPE);

		gui_end_frame(gui);
		{
			frame_milli = time_diff_milli(gui_frame_start(gui), time_current());
			if (frame_milli < 1000.f / FPS_CAP) {
				time_sleep_milli((u32)(1000.f / FPS_CAP) - frame_milli);
				frame_milli = time_diff_milli(gui_frame_start(gui), time_current());
			}
		}
	}

	array_destroy(disolve_effects);
	array_destroy(bg_effects);
err_maps_load:
	array_destroy(maps);
	sound_destroy(&sound_swipe);
	sound_destroy(&sound_success);
err_sound_success:
	sound_destroy(&sound_error);
err_sound_error:
	audio_destroy();
err_audio:
	gui_destroy(gui);
	return 0;
}
