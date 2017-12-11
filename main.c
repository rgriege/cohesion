#include <time.h>

#define VIOLET_IMPLEMENTATION
#include "violet/all.h"

#include "key.h"

#define AUDIO_ENABLED
#ifdef AUDIO_ENABLED
#include <SDL_mixer.h>
#include "audio.h"
#else
#include "audio_stub.h"
#endif

#include "action.h"
#include "history.h"
#include "settings.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define APP_NAME "Cohesion"
#define MAP_DIM_MAX 16
#define MAP_TIP_MAX 64
#define TILE_SIZE 20
#define WALK_SPEED 80
#define CLONE_CNT_MAX 32
#define TILE_BORDER_DIM 1
#define FPS_CAP 30
// #define SHOW_TRAVELLED
#define LEVEL_COMPLETE_EFFECT_DURATION_MILLI 1000
#define ROTATION_EFFECT_DURATION_MILLI 250
#define EDITOR_REPEAT_INTERVAL 150

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
	char tip[MAP_TIP_MAX];
	char desc[MAP_TIP_MAX];
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

struct effect2 {
	v2f pos;
	color_t color;
	r32 t;
	u32 duration;
	r32 rotation_start, rotation_rate;
};

const v2i g_v2i_up    = { .x =  0, .y =  1 };
const v2i g_v2i_down  = { .x =  0, .y = -1 };
const v2i g_v2i_left  = { .x = -1, .y =  0 };
const v2i g_v2i_right = { .x =  1, .y =  0 };

const color_t g_tile_fills[] = {
	gi_nocolor,
	gi_black,
	gi_grey128,
	gi_orange,
	gi_lightblue,
	gi_yellow,
};

static
b32 load_maps(array(struct map) *maps)
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
		if (!vson_read_str(fp, "desc", map.desc, MAP_TIP_MAX))
			goto out;
		if (!vson_read_str(fp, "tip", map.tip, MAP_TIP_MAX))
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
void save_maps(array(const struct map) maps)
{
	FILE *fp = fopen("maps.vson", "w");
	if (!fp) {
		log_error("Failed to open save file");
		return;
	}

	vson_write_u32(fp, "maps", array_sz(maps));
	array_foreach(maps, const struct map, map) {
		vson_write_str(fp, "desc", map->desc);
		vson_write_str(fp, "tip", map->tip);
		vson_write_s32(fp, "width", map->dim.x);
		vson_write_s32(fp, "height", map->dim.y);
		for (s32 i = 0; i < map->dim.y; ++i) {
			for (s32 j = 0; j < map->dim.x; ++j)
				fputc(map->tiles[map->dim.y - i - 1][j].type + '0', fp);
			fputc('\n', fp);
		}
	}
	fclose(fp);
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
void player_entered_tile(struct level *level, struct player *player,
                         u32 *num_clones_attached);

static
void player_init(struct player *player, struct level *level)
{
	u32 num_clones_attached;
	player->tile = level->player_start;
	player->pos = v2i_to_v2f(v2i_scale(player->tile, TILE_SIZE));
	player->dir = DIR_NONE;
	player->num_clones = 0;
	player_entered_tile(level, player, &num_clones_attached);
}

static
b32 tile_walkable(const struct level *level, v2i tile)
{
	if (tile.x < 0 || tile.x >= level->map.dim.x)
		return false;
	if (tile.y < 0 || tile.y >= level->map.dim.y)
		return false;
	if (   level->map.tiles[tile.y][tile.x].type != TILE_HALL
	    && level->map.tiles[tile.y][tile.x].type != TILE_DOOR)
		return false;
	for (u32 i = 0; i < level->num_clones; ++i)
		if (v2i_equal(level->clones[i], tile))
			return false;
	return true;
}

static
b32 tile_walkable_for_player(const struct level *level,
                             const struct player *player, v2i offset)
{
	const v2i tile = v2i_add(player->tile, offset);
	if (!tile_walkable(level, tile))
		return false;
	for (u32 i = 0; i < player->num_clones; ++i)
		if (!tile_walkable(level, v2i_add(tile, player->clones[i])))
			return false;
	return true;
}

static
b32 player_can_rotate(const struct level *level, const struct player *player,
                      b32 clockwise)
{
	v2i(*perp)(v2i) = clockwise ? v2i_rperp : v2i_lperp;
	for (u32 i = 0; i < player->num_clones; ++i)
		if (!tile_walkable(level, v2i_add(player->tile, perp(player->clones[i]))))
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
void player_entered_tile(struct level *level, struct player *player,
                         u32 *num_clones_attached)
{
	const v2i tile = player->tile;
	const u32 num_clones_attached_start = player->num_clones;
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
	*num_clones_attached = player->num_clones - num_clones_attached_start;
}

static
void door_effect_add(array(struct effect2) *door_effects, s32 x, s32 y)
{
	const struct effect2 fx = {
		.pos = { .x = x + rand() % TILE_SIZE, .y = y + rand() % TILE_SIZE },
		.color = g_tile_fills[TILE_DOOR],
		.t = 0.f,
		.duration = 1000 + rand() % 500,
		.rotation_start = rand() % 12, /* approximate 2 * PI */
		.rotation_rate = (rand() % 20) / 3.f - 3.f,
	};
	array_append(*door_effects, fx);
}

static
void dissolve_effect_add(array(struct effect) *effects, v2i offset, v2i tile, color_t fill, u32 duration)
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
				.t = (rand() % 100) / 100.f,
				.duration = 2000 + rand() % 1000,
			};
			array_append(*effects, fx);
		}
	}
}

gui_t *gui;
gui_panel_t settings_panel;
b32 in_editor = false;
b32 quit = false;
u32 level_idx = 0;
struct level level;
struct player player;
u32 frame_milli = 0, time_until_next_door_fx = 0;
struct music music;
struct sound sound_error, sound_slide, sound_swipe, sound_success;
array(struct map) maps;
array(struct effect) bg_effects;
array(struct effect) dissolve_effects;
array(struct effect2) door_effects;
v2i screen, offset;
v2i cursor;
enum action editor_last_action = ACTION_COUNT;
u32 editor_repeat_timer;


void frame(void);
void play(void);
void edit(void);

int main(int argc, char *const argv[]) {
	log_add_std(LOG_STDOUT);

	srand(time(NULL));

	gui = gui_create(0, 0, (MAP_DIM_MAX + 2 * TILE_BORDER_DIM) * TILE_SIZE,
	                (MAP_DIM_MAX + 2 * TILE_BORDER_DIM) * TILE_SIZE,
	                APP_NAME, WINDOW_CENTERED);
	if (!gui)
		return 1;

	pgui_panel_init(gui, &settings_panel, TILE_SIZE, TILE_SIZE,
	                MAP_DIM_MAX * TILE_SIZE, MAP_DIM_MAX * TILE_SIZE,
	                "", GUI_PANEL_CLOSABLE | GUI_PANEL_SCROLLBARS);
	hide_settings(&settings_panel);

	gui_dim(gui, &screen.x, &screen.y);

	if (!audio_init())
		goto err_audio;

#ifdef __EMSCRIPTEN__
	check(sound_init(&sound_error, "error.mp3"));
	check(sound_init(&sound_slide, "slide.mp3"));
	check(sound_init(&sound_swipe, "swipe.mp3"));
	check(sound_init(&sound_success, "success.mp3"));
	check(music_init(&music, "score.mp3"));
#else
	check(sound_init(&sound_error, "error.aiff"));
	check(sound_init(&sound_slide, "slide.aiff"));
	check(sound_init(&sound_swipe, "swipe.aiff"));
	check(sound_init(&sound_success, "success.aiff"));
	check(music_init(&music, "score.aiff"));
#endif

	maps = array_create();
	if (!load_maps(&maps) || array_empty(maps))
		goto err_maps_load;

	level_init(&level, &maps[level_idx]);
	player_init(&player, &level);

	load_settings();

	bg_effects = array_create();
	background_generate(&bg_effects, screen);

	dissolve_effects = array_create();

	door_effects = array_create();

	music_play(&music);

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(frame, 0, 0);
	return 0;
#else
	while (!quit) {
		frame();
		frame_milli = time_diff_milli(gui_frame_start(gui), time_current());
		if (frame_milli < (u32)(1000.f / FPS_CAP))
			time_sleep_milli((u32)(1000.f / FPS_CAP) - frame_milli);
	}
#endif

	array_destroy(door_effects);
	array_destroy(dissolve_effects);
	array_destroy(bg_effects);
err_maps_load:
	array_destroy(maps);
	sound_destroy(&sound_swipe);
	sound_destroy(&sound_slide);
	sound_destroy(&sound_success);
	sound_destroy(&sound_error);
	music_destroy(&music);
	audio_destroy();
err_audio:
	gui_destroy(gui);
	return 0;
}

void frame(void)
{
	if (!gui_begin_frame(gui)) {
		quit = true;
		return;
	}

	frame_milli = gui_frame_time_milli(gui);

	gui_dim(gui, &screen.x, &screen.y);
	offset = v2i_scale_inv(v2i_sub(screen, v2i_scale(level.map.dim, TILE_SIZE)), 2);

	if (!settings_panel.hidden)
		show_settings(gui, &settings_panel);

	if (in_editor)
		edit();
	else
		play();

	quit = key_down(gui, KB_ESCAPE);

	gui_end_frame(gui);
}

void play(void)
{
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

	{
		const s32 x = offset.x + level.map.dim.x * TILE_SIZE / 2;
		const s32 y = offset.y + level.map.dim.y * TILE_SIZE;
		char buf[16];
		snprintf(buf, 16, "Level %u", level_idx + 1);
		gui_txt(gui, x, y + 10, 20, buf, g_white, GUI_ALIGN_CENTER);
		if (level_idx == 0)
			gui_txt(gui, x, y + 60, 32, APP_NAME, g_white, GUI_ALIGN_CENTER);
	}

	{
		const s32 dim = 30;
		const s32 y = screen.y - dim;
		const s32 border = 5;
		const s32 y0_disabled = y + border;
		const s32 y1_disabled = y + dim - border;
		s32 x = screen.x - 90;

		gui_style_push(gui, btn, g_gui_style_invis.btn);

		if (gui_btn_img(gui, x, y, 30, 30, "music.png", IMG_CENTERED) == BTN_PRESS) {
			music_toggle();
			save_settings();
		}
		if (!music_enabled()) {
			gui_line(gui, x + border, y0_disabled, x + dim - border, y1_disabled, 2, g_red);
			gui_line(gui, x + border, y1_disabled, x + dim - border, y0_disabled, 2, g_red);
		}

		x += dim;

		if (gui_btn_img(gui, x, y, 30, 30, "sound.png", IMG_CENTERED) == BTN_PRESS) {
			sound_toggle();
			save_settings();
		}
		if (!sound_enabled()) {
			gui_line(gui, x + border, y0_disabled, x + dim - border, y1_disabled, 2, g_red);
			gui_line(gui, x + border, y1_disabled, x + dim - border, y0_disabled, 2, g_red);
		}

		x += dim;

		if (gui_btn_img(gui, x, y, 30, 30, "settings.png", IMG_CENTERED) == BTN_PRESS) {
			if (settings_panel.hidden)
				settings_panel.hidden = false;
			else
				hide_settings(&settings_panel);
		}
		gui_style_pop(gui);
	}

	gui_txt(gui, offset.x + level.map.dim.x * TILE_SIZE / 2, offset.y - 20, 14,
	        level.map.tip, g_white, GUI_ALIGN_CENTER);

	if (!level.complete) {
		gui_style_push(gui, btn, g_gui_style_invis.btn);
		if (gui_btn_img(gui, offset.x + level.map.dim.x * TILE_SIZE / 2 - 15,
		                offset.y - 50, 30, 30, "reset.png", IMG_CENTERED) == BTN_PRESS) {
			history_clear();
			level_init(&level, &maps[level_idx]);
			player_init(&player, &level);
		}
		gui_style_pop(gui);
	}

	if (!level.complete) {
		for (s32 i = 0; i < level.map.dim.y; ++i) {
			const s32 y = offset.y + i * TILE_SIZE;
			for (s32 j = 0; j < level.map.dim.x; ++j) {
				const s32 x = offset.x + j * TILE_SIZE;
				const enum tile_type type = level.map.tiles[i][j].type;
				switch (type) {
				case TILE_BLANK:
				break;
				case TILE_HALL:
				case TILE_PLAYER:
				case TILE_CLONE:
					gui_rect(gui, x, y, TILE_SIZE, TILE_SIZE,
					         g_tile_fills[TILE_HALL], gui_style(gui)->bg_color);
				break;
				case TILE_WALL:
					gui_rect(gui, x, y, TILE_SIZE, TILE_SIZE,
					         g_tile_fills[TILE_WALL], gui_style(gui)->bg_color);
				break;
				case TILE_DOOR:
					gui_rect(gui, x, y, TILE_SIZE, TILE_SIZE,
					         g_tile_fills[TILE_DOOR], gui_style(gui)->bg_color);
					if (frame_milli >= time_until_next_door_fx) {
						door_effect_add(&door_effects, x, y);
						time_until_next_door_fx = 100 + rand() % 100;
					} else {
						time_until_next_door_fx -= frame_milli;
					}
				break;
				}
#ifdef SHOW_TRAVELLED
				if (level.map.tiles[i][j].travelled)
					gui_circ(gui, x + TILE_SIZE / 2, y + TILE_SIZE / 2, TILE_SIZE / 4,
					         g_red, g_nocolor);
#endif
			}
		}
	}

	for (u32 i = 0; i < array_sz(door_effects); ) {
		struct effect2 *fx = &door_effects[i];
		const r32 delta = (r32)frame_milli / fx->duration;
		fx->t += delta;
		if (fx->t <= 1.f) {
			const r32 sz = TILE_SIZE / 2 * fx->t;
			const m3f m = m3f_init_rot(fx->rotation_start + fx->rotation_rate * fx->t);
			v2f square[4] = {
				v2f_add(m3f_mul_v2(m, (v2f){ .x =  sz / 2, .y =  sz / 2 }), fx->pos),
				v2f_add(m3f_mul_v2(m, (v2f){ .x = -sz / 2, .y =  sz / 2 }), fx->pos),
				v2f_add(m3f_mul_v2(m, (v2f){ .x = -sz / 2, .y = -sz / 2 }), fx->pos),
				v2f_add(m3f_mul_v2(m, (v2f){ .x =  sz / 2, .y = -sz / 2 }), fx->pos),
			};
			color_t fill = fx->color;
			fill.a = 48 * fx->t + 48;
			gui_polyf(gui, square, 4, fill, g_nocolor);
			++i;
		} else if (fx->t <= 2.f) {
			const r32 sz = TILE_SIZE / 2 * (2.f - fx->t);
			const m3f m = m3f_init_rot(fx->rotation_start + fx->rotation_rate * fx->t);
			v2f square[4] = {
				v2f_add(m3f_mul_v2(m, (v2f){ .x =  sz / 2, .y =  sz / 2 }), fx->pos),
				v2f_add(m3f_mul_v2(m, (v2f){ .x = -sz / 2, .y =  sz / 2 }), fx->pos),
				v2f_add(m3f_mul_v2(m, (v2f){ .x = -sz / 2, .y = -sz / 2 }), fx->pos),
				v2f_add(m3f_mul_v2(m, (v2f){ .x =  sz / 2, .y = -sz / 2 }), fx->pos),
			};
			color_t fill = fx->color;
			fill.a = 48 * (2.f - fx->t) + 48;
			gui_polyf(gui, square, 4, fill, g_nocolor);
			++i;
		} else {
			array_remove_fast(door_effects, i);
		}
	}

	for (u32 i = 0; i < array_sz(dissolve_effects); ) {
		struct effect *fx = &dissolve_effects[i];
		fx->t += (r32)frame_milli / fx->duration;
		if (fx->t <= 1.f) {
			const u32 sz = (TILE_SIZE - 4) * (1.f - fx->t);
			color_t fill = fx->color;
			fill.a = 255 * (1.f - fx->t);
			gui_rect(gui, fx->pos.x - sz / 2, fx->pos.y - sz / 2, sz, sz, fill, g_nocolor);
			++i;
		} else {
			array_remove_fast(dissolve_effects, i);
		}
	}

	if (!level.complete)
		for (u32 i = 0; i < level.num_clones; ++i)
			render_player(gui, offset, v2i_scale(level.clones[i], TILE_SIZE),
			              g_tile_fills[TILE_CLONE]);

	switch (player.dir) {
	case DIR_NONE:
		if (   key_down(gui, g_key_bindings[ACTION_MOVE_UP])
		    && settings_panel.hidden
		    && tile_walkable_for_player(&level, &player, g_v2i_up)) {
			player.dir = DIR_UP;
			++player.tile.y;
			sound_play(&sound_slide);
		} else if (   key_down(gui, g_key_bindings[ACTION_MOVE_DOWN])
		           && settings_panel.hidden
		           && tile_walkable_for_player(&level, &player, g_v2i_down)) {
			player.dir = DIR_DOWN;
			--player.tile.y;
			sound_play(&sound_slide);
		} else if (   key_down(gui, g_key_bindings[ACTION_MOVE_LEFT])
		           && settings_panel.hidden
		           && tile_walkable_for_player(&level, &player, g_v2i_left)) {
			player.dir = DIR_LEFT;
			--player.tile.x;
			sound_play(&sound_slide);
		} else if (   key_down(gui, g_key_bindings[ACTION_MOVE_RIGHT])
		           && settings_panel.hidden
		           && tile_walkable_for_player(&level, &player, g_v2i_right)) {
			player.dir = DIR_RIGHT;
			++player.tile.x;
			sound_play(&sound_slide);
		} else if (key_pressed(gui, g_key_bindings[ACTION_ROTATE_CCW])) {
			if (player_can_rotate(&level, &player, false)) {
				u32 num_clones_attached;
				for (u32 i = 0; i < player.num_clones; ++i) {
					dissolve_effect_add(&dissolve_effects, offset,
					                    v2i_add(player.tile, player.clones[i]),
					                    g_tile_fills[TILE_CLONE],
					                    ROTATION_EFFECT_DURATION_MILLI);
					player.clones[i] = v2i_lperp(player.clones[i]);
				}
				player_entered_tile(&level, &player, &num_clones_attached);
				history_push(ACTION_ROTATE_CCW, num_clones_attached);
				sound_play(&sound_swipe);
			} else {
				sound_play(&sound_error);
			}
		} else if (key_pressed(gui, g_key_bindings[ACTION_ROTATE_CW])) {
			if (player_can_rotate(&level, &player, true)) {
				u32 num_clones_attached;
				for (u32 i = 0; i < player.num_clones; ++i) {
					dissolve_effect_add(&dissolve_effects, offset,
					                    v2i_add(player.tile, player.clones[i]),
					                    g_tile_fills[TILE_CLONE],
					                    ROTATION_EFFECT_DURATION_MILLI);
					player.clones[i] = v2i_rperp(player.clones[i]);
				}
				player_entered_tile(&level, &player, &num_clones_attached);
				history_push(ACTION_ROTATE_CW, num_clones_attached);
				sound_play(&sound_swipe);
			} else {
				sound_play(&sound_error);
			}
		} else if (   key_pressed(gui, g_key_bindings[ACTION_UNDO])
		           && settings_panel.hidden) {
			enum action action;
			u32 num_clones;
			if (history_pop(&action, &num_clones)) {
				for (u32 i = 0; i < num_clones; ++i) {
					const v2i tile_relative = player.clones[--player.num_clones];
					const v2i tile_absolute = v2i_add(player.tile, tile_relative);
					level.clones[level.num_clones++] = tile_absolute;
				}
				switch (action) {
				case ACTION_MOVE_UP:
					--player.tile.y;
					player.pos.y = player.tile.y * TILE_SIZE;
				break;
				case ACTION_MOVE_DOWN:
					++player.tile.y;
					player.pos.y = player.tile.y * TILE_SIZE;
				break;
				case ACTION_MOVE_LEFT:
					++player.tile.x;
					player.pos.x = player.tile.x * TILE_SIZE;
				break;
				case ACTION_MOVE_RIGHT:
					--player.tile.x;
					player.pos.x = player.tile.x * TILE_SIZE;
				break;
				case ACTION_ROTATE_CW:
					for (u32 i = 0; i < player.num_clones; ++i) {
						dissolve_effect_add(&dissolve_effects, offset,
						                    v2i_add(player.tile, player.clones[i]),
						                    g_tile_fills[TILE_CLONE],
						                    ROTATION_EFFECT_DURATION_MILLI);
						player.clones[i] = v2i_lperp(player.clones[i]);
					}
				break;
				case ACTION_ROTATE_CCW:
				for (u32 i = 0; i < player.num_clones; ++i) {
					dissolve_effect_add(&dissolve_effects, offset,
					                    v2i_add(player.tile, player.clones[i]),
					                    g_tile_fills[TILE_CLONE],
					                    ROTATION_EFFECT_DURATION_MILLI);
					player.clones[i] = v2i_rperp(player.clones[i]);
				}
				break;
				case ACTION_UNDO:
				case ACTION_RESET:
				case ACTION_COUNT:
					assert(false);
				break;
				}
			} else {
				sound_play(&sound_error);
			}
		} else if (   key_pressed(gui, g_key_bindings[ACTION_RESET])
		           && settings_panel.hidden) {
			history_clear();
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
		} else if (key_pressed(gui, KB_N)) {
			history_clear();
			level_idx = (level_idx + 1) % array_sz(maps);
			level_init(&level, &maps[level_idx]);
			player_init(&player, &level);
		} else if (key_pressed(gui, KB_P)) {
			history_clear();
			level_idx = (level_idx + array_sz(maps) - 1) % array_sz(maps);
			level_init(&level, &maps[level_idx]);
			player_init(&player, &level);
		}
	break;
	case DIR_UP:
		player.pos.y += WALK_SPEED * frame_milli / 1000.f;
		if ((s32)player.pos.y >= player.tile.y * TILE_SIZE) {
			u32 num_clones_attached;
			player_entered_tile(&level, &player, &num_clones_attached);
			history_push(ACTION_MOVE_UP, num_clones_attached);
			if (   key_down(gui, g_key_bindings[ACTION_MOVE_UP])
			    && settings_panel.hidden
			    && tile_walkable_for_player(&level, &player, g_v2i_up)) {
				++player.tile.y;
				sound_play(&sound_slide);
			} else {
				player.pos.y = player.tile.y * TILE_SIZE;
				player.dir = DIR_NONE;
			}
		}
	break;
	case DIR_DOWN:
		player.pos.y -= WALK_SPEED * frame_milli / 1000.f;
		if ((s32)player.pos.y <= player.tile.y * TILE_SIZE) {
			u32 num_clones_attached;
			player_entered_tile(&level, &player, &num_clones_attached);
			history_push(ACTION_MOVE_DOWN, num_clones_attached);
			if (   key_down(gui, g_key_bindings[ACTION_MOVE_DOWN])
			    && settings_panel.hidden
			    && tile_walkable_for_player(&level, &player, g_v2i_down)) {
				--player.tile.y;
				sound_play(&sound_slide);
			} else {
				player.pos.y = player.tile.y * TILE_SIZE;
				player.dir = DIR_NONE;
			}
		}
	break;
	case DIR_LEFT:
		player.pos.x -= WALK_SPEED * frame_milli / 1000.f;
		if ((s32)player.pos.x <= player.tile.x * TILE_SIZE) {
			u32 num_clones_attached;
			player_entered_tile(&level, &player, &num_clones_attached);
			history_push(ACTION_MOVE_LEFT, num_clones_attached);
			if (   key_down(gui, g_key_bindings[ACTION_MOVE_LEFT])
			    && settings_panel.hidden
			    && tile_walkable_for_player(&level, &player, g_v2i_left)) {
				--player.tile.x;
				sound_play(&sound_slide);
			} else {
				player.pos.x = player.tile.x * TILE_SIZE;
				player.dir = DIR_NONE;
			}
		}
	break;
	case DIR_RIGHT:
		player.pos.x += WALK_SPEED * frame_milli / 1000.f;
		if ((s32)player.pos.x >= player.tile.x * TILE_SIZE) {
			u32 num_clones_attached;
			player_entered_tile(&level, &player, &num_clones_attached);
			history_push(ACTION_MOVE_RIGHT, num_clones_attached);
			if (   key_down(gui, g_key_bindings[ACTION_MOVE_RIGHT])
			    && settings_panel.hidden
			    && tile_walkable_for_player(&level, &player, g_v2i_right)) {
				++player.tile.x;
				sound_play(&sound_slide);
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
				if (level.map.tiles[i][j].type != TILE_BLANK)
					dissolve_effect_add(&dissolve_effects, offset, tile, g_tile_fills[level.map.tiles[i][j].type], LEVEL_COMPLETE_EFFECT_DURATION_MILLI);
			}
		}
		dissolve_effect_add(&dissolve_effects, offset, player.tile, g_tile_fills[TILE_PLAYER], LEVEL_COMPLETE_EFFECT_DURATION_MILLI);
		for (u32 i = 0; i < player.num_clones; ++i)
			dissolve_effect_add(&dissolve_effects, offset, v2i_add(player.tile, player.clones[i]), g_tile_fills[TILE_CLONE], LEVEL_COMPLETE_EFFECT_DURATION_MILLI);
		sound_play(&sound_success);
		level.complete = true;
		array_foreach(door_effects, struct effect2, fx)
			if (fx->t < 1.f)
				fx->t = 2.f - fx->t;
	}

	if (level.complete && array_empty(dissolve_effects)) {
		history_clear();
		level_idx = (level_idx + 1) % array_sz(maps);
		level_init(&level, &maps[level_idx]);
		player_init(&player, &level);
		background_generate(&bg_effects, screen);
	}

	if (key_pressed(gui, KB_F1) && !is_key_bound(KB_F1)) {
		struct map map_edit = { .dim = { MAP_DIM_MAX, MAP_DIM_MAX } };
		const v2i offset_ = v2i_scale_inv(v2i_sub(map_edit.dim, level.map.dim), 2);
		for (s32 i = 0; i < offset_.y; ++i)
			for (s32 j = 0; j < map_edit.dim.x; ++j)
				map_edit.tiles[i][j].type = TILE_BLANK;
		for (s32 i = offset_.y; i < offset_.y + level.map.dim.y; ++i) {
			for (s32 j = 0; j < offset_.x; ++j)
				map_edit.tiles[i][j].type = TILE_BLANK;
			for (s32 j = offset_.x; j < offset_.x + level.map.dim.x; ++j)
				map_edit.tiles[i][j] = maps[level_idx].tiles[i - offset_.y][j - offset_.x];
			for (s32 j = offset_.x + level.map.dim.x; j < map_edit.dim.x; ++j)
				map_edit.tiles[i][j].type = TILE_BLANK;
		}
		for (s32 i = offset_.y + level.map.dim.y; i < map_edit.dim.y; ++i)
			for (s32 j = 0; j < map_edit.dim.x; ++j)
				map_edit.tiles[i][j].type = TILE_BLANK;
		level.map.dim = map_edit.dim;
		memcpy(level.map.tiles, map_edit.tiles, sizeof(map_edit.tiles));
		in_editor = true;
		cursor = offset_;
		history_clear();
	}
}

static
b32 editor_attempt_action(enum action action)
{
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

void edit(void)
{
	if (key_pressed(gui, KB_F1) && !is_key_bound(KB_F1)) {
		s32 min_i = MAP_DIM_MAX, min_j = MAP_DIM_MAX;
		s32 max_i = 0, max_j = 0;
		struct map map_play;
		for (s32 i = 0; i < level.map.dim.y; ++i) {
			for (s32 j = 0; j < level.map.dim.x; ++j) {
				if (level.map.tiles[i][j].type != TILE_BLANK) {
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
				map_play.tiles[i][j] = level.map.tiles[i + min_i][j + min_j];
		maps[level_idx].dim = map_play.dim;
		memcpy(maps[level_idx].tiles, map_play.tiles, sizeof(map_play.tiles));
		level_init(&level, &maps[level_idx]);
		player_init(&player, &level);
		in_editor = false;
		offset = v2i_scale_inv(v2i_sub(screen, v2i_scale(level.map.dim, TILE_SIZE)), 2);
	}

	if (editor_attempt_action(ACTION_MOVE_UP)) {
		cursor.y = min(cursor.y + 1, MAP_DIM_MAX - 1);
	} else if (editor_attempt_action(ACTION_MOVE_DOWN)) {
		cursor.y = max(cursor.y - 1, 0);
	} else if (editor_attempt_action(ACTION_MOVE_LEFT)) {
		cursor.x = max(cursor.x - 1, 0);
	} else if (editor_attempt_action(ACTION_MOVE_RIGHT)) {
		cursor.x = min(cursor.x + 1, MAP_DIM_MAX - 1);
	} else if (editor_attempt_action(ACTION_ROTATE_CW)) {
		level.map.tiles[cursor.y][cursor.x].type
			= (level.map.tiles[cursor.y][cursor.x].type + 1) % (TILE_DOOR + 1);
		save_maps(maps);
	} else if (editor_attempt_action(ACTION_ROTATE_CCW)) {
		level.map.tiles[cursor.y][cursor.x].type
			= (level.map.tiles[cursor.y][cursor.x].type + TILE_DOOR) % (TILE_DOOR + 1);
		save_maps(maps);
	}

	if (gui_npt(gui, 2, screen.y - TILE_SIZE + 2, screen.x - 4, TILE_SIZE - 4,
	            level.map.desc, MAP_TIP_MAX - 1, "description", 0))
		save_maps(maps);

	for (s32 i = 0; i < level.map.dim.y; ++i) {
		const s32 y = offset.y + i * TILE_SIZE;
		for (s32 j = 0; j < level.map.dim.x; ++j) {
			const s32 x = offset.x + j * TILE_SIZE;
			const enum tile_type type = level.map.tiles[i][j].type;
			const color_t color = { .r=0x32, .g=0x2f, .b=0x2f, .a=0xff };
			gui_rect(gui, x, y, TILE_SIZE, TILE_SIZE,
			         g_tile_fills[type], color);
		}
	}

	if (gui_npt(gui, 2, 2, screen.x - 4, TILE_SIZE - 4,
	            level.map.tip, MAP_TIP_MAX - 1, "tip", 0))
		save_maps(maps);

	gui_rect(gui, offset.x + cursor.x * TILE_SIZE, offset.y + cursor.y * TILE_SIZE,
	         TILE_SIZE, TILE_SIZE, g_nocolor, g_white);
}
