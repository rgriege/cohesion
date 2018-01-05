#include <time.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include "config.h"
#define VIOLET_IMPLEMENTATION
#include "violet/all.h"
#include "key.h"
#include "audio.h"
#include "action.h"
#include "types.h"
#include "constants.h"
#include "history.h"
#include "settings.h"
#include "disk.h"
#include "actor.h"
#include "player.h"
#include "level.h"
#include "editor.h"

/* TODO: find a better way to to do play/fx globals */
struct
{
	const u32 *level_idx;
	array(struct map) *maps;
	const v2i *offset;
	struct sound *sound_error, *sound_slide, *sound_swipe, *sound_success;
	array(struct effect) *dissolve_effects;
	struct player *players;
	const u32 *num_players;
} glob;

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
b32 can_execute_solo_action(enum action action, struct player *player,
                            const struct level *level)
{
	if (action == ACTION_UNDO) {
		b32 success = true;
		struct history history_temp;
		enum action a;
		u32 num_clones;

		history_clear(&history_temp);
		for (u32 i = 0; i < player->num_actors; ++i) {
			const struct actor *actor = player->actors[player->num_actors - i - 1];
			if (!history_pop(&player->history, &a, &num_clones)) {
				success = false;
			} else {
				success &= actor_can_undo(actor, level, a, num_clones);
				history_push(&history_temp, a, num_clones);
			}
		}
		while (history_pop(&history_temp, &a, &num_clones))
			history_push(&player->history, a, num_clones);
		return success;
	} else {
		for (u32 j = 0; j < player->num_actors; ++j)
			if (!actor_can_act(player->actors[j], level, action))
				return false;
	}
	return true;
}

static
void dissolve_effect_add(array(struct effect) *effects, v2i offset, v2i tile,
                         color_t fill, u32 duration)
{
#ifdef SHOW_EFFECTS
	static const v2i half_time = { .x = TILE_SIZE / 2, .y = TILE_SIZE / 2 };
	const struct effect fx = {
		.pos = v2i_add(v2i_add(offset, v2i_scale(tile, TILE_SIZE)), half_time),
		.color = fill,
		.t = 0.f,
		.duration = duration,
	};
	array_append(*effects, fx);
#endif
}

static
void undo(struct player *player, struct level *level)
{
	for (u32 i = 0; i < player->num_actors; ++i) {
		struct actor *actor = player->actors[player->num_actors - i - 1];
		enum action action;
		u32 num_clones;
		if (history_pop(&player->history, &action, &num_clones)) {
			for (u32 j = 0; j < num_clones; ++j) {
				const v2i tile_relative = actor->clones[--actor->num_clones];
				const v2i tile_absolute = v2i_add(actor->tile, tile_relative);
				level->clones[level->num_clones++] = tile_absolute;
			}
			switch (action) {
			case ACTION_MOVE_UP:
				--actor->tile.y;
				actor->pos.y = actor->tile.y * TILE_SIZE;
			break;
			case ACTION_MOVE_DOWN:
				++actor->tile.y;
				actor->pos.y = actor->tile.y * TILE_SIZE;
			break;
			case ACTION_MOVE_LEFT:
				++actor->tile.x;
				actor->pos.x = actor->tile.x * TILE_SIZE;
			break;
			case ACTION_MOVE_RIGHT:
				--actor->tile.x;
				actor->pos.x = actor->tile.x * TILE_SIZE;
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
void execute_action(enum action action, struct player *player, struct level *level)
{
	enum dir dir;
	struct actor *actor;
	u32 num_clones_attached;

	switch (action) {
	case ACTION_MOVE_UP:
	case ACTION_MOVE_DOWN:
	case ACTION_MOVE_LEFT:
	case ACTION_MOVE_RIGHT:
		dir = g_action_dir[action];
		for (u32 i = 0; i < player->num_actors; ++i) {
			actor = player->actors[i];
			actor->dir = dir;
			v2i_add_eq(&actor->tile, g_dir_vec[dir]);
		}
		sound_play(glob.sound_slide);
	break;
	case ACTION_ROTATE_CCW:
	case ACTION_ROTATE_CW:
		for (u32 i = 0; i < player->num_actors; ++i) {
			actor = player->actors[i];
			for (u32 j = 0; j < actor->num_clones; ++j) {
				dissolve_effect_add(glob.dissolve_effects, *glob.offset,
														v2i_add(actor->tile, actor->clones[j]),
														g_tile_fills[TILE_CLONE],
														ROTATION_EFFECT_DURATION_MILLI);
				actor->clones[j] = perp(actor->clones[j], action);
			}
			actor_entered_tile(actor, level, &num_clones_attached);
			history_push(&player->history, action, num_clones_attached);
		}
		sound_play(glob.sound_swipe);
	break;
	case ACTION_UNDO:
		undo(player, level);
		for (u32 i = 0; i < *glob.num_players; ++i) {
			for (u32 j = 0; j < glob.players[i].num_actors; ++j) {
				u32 num_clones_attached;
				actor_entered_tile(glob.players[i].actors[j], level, &num_clones_attached);
				if (num_clones_attached) {
					enum action a;
					u32 num_clones;
					assert(history_pop(&glob.players[i].history, &a, &num_clones));
					history_push(&glob.players[i].history, a, num_clones_attached + num_clones);
				}
			}
		}
	break;
	case ACTION_RESET:
		history_clear(&player->history);
		/*if (key_mod(gui, KBM_CTRL)) {
			const struct map current_map = (*glob.maps)[*glob.level_idx];
			if (!load_maps(g_current_maps_file_name, glob.maps)) {
				array_append(*glob.maps, current_map);
				*glob.level_idx = 0;
			}
		}*/
		level_init(level, glob.players, &(*glob.maps)[*glob.level_idx]);
	break;
	case ACTION_COUNT:
		assert(false);
	}
}

static
void render_actor(gui_t *gui, v2i offset, v2i pos, color_t color)
{
	gui_rect(gui, offset.x + 2 + pos.x, offset.y + 2 + pos.y,
	         TILE_SIZE - 4, TILE_SIZE - 4, color, g_nocolor);
}

static
void door_effect_add(array(struct effect2) *door_effects, s32 x, s32 y)
{
#ifdef SHOW_EFFECTS
	const struct effect2 fx = {
		.pos = { .x = x + rand() % TILE_SIZE, .y = y + rand() % TILE_SIZE },
		.color = g_tile_fills[TILE_DOOR],
		.t = 0.f,
		.duration = 1000 + rand() % 500,
		.rotation_start = rand() % 12, /* approximate 2 * PI */
		.rotation_rate = (rand() % 20) / 3.f - 3.f,
	};
	array_append(*door_effects, fx);
#endif
}

static
void background_generate(array(struct effect) *effects, v2i screen)
{
#ifdef SHOW_EFFECTS
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
#endif
}

static
void move_actors(struct level *level, struct player players[],
                 u32 frame_milli, u32 milli_consumed[])
{
	for (u32 i = 0; i < level->num_actors; ++i) {
		struct actor *actor = &level->actors[i];
		r32 pos;
		s32 dst;
		const u32 walk_milli = frame_milli - milli_consumed[i];

		switch (actor->dir) {
		case DIR_NONE:
		break;
		case DIR_UP:
			dst = actor->tile.y * TILE_SIZE;
			pos = actor->pos.y + WALK_SPEED * walk_milli / 1000.f;
			if ((s32)pos >= dst) {
				u32 num_clones_attached;
				struct player *player = &players[level->map.actor_controlled_by_player[i]];
				actor_entered_tile(actor, level, &num_clones_attached);
				history_push(&player->history, ACTION_MOVE_UP, num_clones_attached);
				milli_consumed[i] = frame_milli -   1000.f * ((s32)pos - dst)
				                                  / WALK_SPEED;
				actor->pos.y = dst;
				actor->dir = DIR_NONE;
			} else {
				milli_consumed[i] = frame_milli;
				actor->pos.y = pos;
			}
		break;
		case DIR_DOWN:
			dst = actor->tile.y * TILE_SIZE;
			pos = actor->pos.y - WALK_SPEED * walk_milli / 1000.f;
			if ((s32)pos <= dst) {
				struct player *player = &players[level->map.actor_controlled_by_player[i]];
				u32 num_clones_attached;
				actor_entered_tile(actor, level, &num_clones_attached);
				history_push(&player->history, ACTION_MOVE_DOWN, num_clones_attached);
				milli_consumed[i] = frame_milli -   1000.f * (dst - (s32)pos)
				                                  / WALK_SPEED;
				actor->pos.y = dst;
				actor->dir = DIR_NONE;
			} else {
				milli_consumed[i] = frame_milli;
				actor->pos.y = pos;
			}
		break;
		case DIR_LEFT:
			dst = actor->tile.x * TILE_SIZE;
			pos = actor->pos.x - WALK_SPEED * walk_milli / 1000.f;
			if ((s32)pos <= dst) {
				struct player *player = &players[level->map.actor_controlled_by_player[i]];
				u32 num_clones_attached;
				actor_entered_tile(actor, level, &num_clones_attached);
				history_push(&player->history, ACTION_MOVE_LEFT, num_clones_attached);
				milli_consumed[i] = frame_milli -   1000.f * (dst - (s32)pos)
				                                  / WALK_SPEED;
				actor->pos.x = dst;
				actor->dir = DIR_NONE;
			} else {
				milli_consumed[i] = frame_milli;
				actor->pos.x = pos;
			}
		break;
		case DIR_RIGHT:
			dst = actor->tile.x * TILE_SIZE;
			pos = actor->pos.x + WALK_SPEED * walk_milli / 1000.f;
			if ((s32)pos >= dst) {
				struct player *player = &players[level->map.actor_controlled_by_player[i]];
				u32 num_clones_attached;
				actor_entered_tile(actor, level, &num_clones_attached);
				history_push(&player->history, ACTION_MOVE_RIGHT, num_clones_attached);
				milli_consumed[i] = frame_milli -   1000.f * ((s32)pos - dst)
				                                  / WALK_SPEED;
				actor->pos.x = dst;
				actor->dir = DIR_NONE;
			} else {
				milli_consumed[i] = frame_milli;
				actor->pos.x = pos;
			}
		break;
		}
	}
}

static
b32 all_other_players_pending_action(struct player players[], u32 num_players, u32 excluded_player, enum action action)
{
	for (u32 i = 0; i < num_players; ++i)
		if (i != excluded_player && players[i].pending_action != action)
			return false;
	return true;
}

enum mode {
	MENU,
	PLAY,
	EDIT,
};

gui_t *gui;
gui_panel_t settings_panel;
enum mode mode = MENU;
b32 quit = false;
u32 level_idx = 0;
struct level level;
struct player players[PLAYER_CNT_MAX];
u32 num_players;
u32 time_until_next_door_fx = 0;
struct music music;
struct sound sound_error, sound_slide, sound_swipe, sound_success;
array(struct map) maps;
array(struct effect) bg_effects;
array(struct effect) dissolve_effects;
array(struct effect2) door_effects;
v2i screen, offset;
v2i cursor;
const char *g_solo_maps_file_name = "maps.vson";
const char *g_coop_maps_file_name = "maps_coop.vson";
char g_current_maps_file_name[128];


void frame(void);
void menu(u32 frame_milli);
void play(u32 frame_milli);

int main(int argc, char *const argv[])
{
	log_add_std(LOG_STDOUT);

	srand(time(NULL));

	gui = gui_create(0, 0, (MAP_DIM_MAX + 2 * TILE_BORDER_DIM) * TILE_SIZE,
	                (MAP_DIM_MAX + 2 * TILE_BORDER_DIM) * TILE_SIZE,
	                APP_NAME, WINDOW_CENTERED);
	if (!gui)
		return 1;

	pgui_panel_init(gui, &settings_panel, TILE_SIZE, 2 * TILE_SIZE,
	                MAP_DIM_MAX * TILE_SIZE, (MAP_DIM_MAX - 2) * TILE_SIZE,
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

	load_settings();

	for (u32 i = 0; i < PLAYER_CNT_MAX; ++i)
		player_init(&players[i], i);

	bg_effects = array_create();
	background_generate(&bg_effects, screen);

	dissolve_effects = array_create();

	door_effects = array_create();

	glob.level_idx = &level_idx;
	glob.sound_error = &sound_error;
	glob.sound_slide = &sound_slide;
	glob.sound_swipe = &sound_swipe;
	glob.sound_success = &sound_success;
	glob.maps = &maps;
	glob.dissolve_effects = &dissolve_effects;
	glob.offset = &offset;
	glob.players = players;
	glob.num_players = &num_players;

	music_play(&music);

	editor_init();

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(frame, 0, 0);
	return 0;
#else
	while (!quit) {
		u32 frame_milli, fps;
		frame();
		frame_milli = time_diff_milli(gui_frame_start(gui), time_current());
#ifdef SHOW_EFFECTS
		fps =   mode == EDIT && time_diff_milli(gui_last_input_time(gui), time_current())
			    > IDLE_TIMER_MILLI ? FPS_IDLE : FPS_CAP;
#else
		fps =   time_diff_milli(gui_last_input_time(gui), time_current())
			    > IDLE_TIMER_MILLI ? FPS_IDLE : FPS_CAP;
#endif
		if (frame_milli < (u32)(1000.f / fps))
			time_sleep_milli((u32)(1000.f / fps) - frame_milli);
	}
#endif

	array_destroy(door_effects);
	array_destroy(dissolve_effects);
	array_destroy(bg_effects);
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
	u32 frame_milli;

	if (!gui_begin_frame(gui)) {
		quit = true;
		return;
	}

	frame_milli = gui_frame_time_milli(gui);

	gui_dim(gui, &screen.x, &screen.y);
	offset = v2i_scale_inv(v2i_sub(screen, v2i_scale(level.map.dim, TILE_SIZE)), 2);

	if (!settings_panel.hidden)
		show_settings(gui, &settings_panel);

	switch (mode) {
	case MENU:
		menu(frame_milli);
		quit |= key_pressed(gui, KB_ESCAPE);
	break;
	case PLAY:
		play(frame_milli);
		if (key_pressed(gui, KB_ESCAPE))
			mode = MENU;
	break;
	case EDIT:;
		u32 level_to_play;
		editor_update(gui, &level_to_play);
		if (level_to_play < array_sz(maps)) {
			if (   g_current_maps_file_name[0] != '\0'
			    || file_save_dialog(g_current_maps_file_name, 128, "vson"))
				save_maps(g_current_maps_file_name, maps);
			level_idx = level_to_play;
			level_init(&level, players, &maps[level_idx]);
			mode = PLAY;
		} else if (key_pressed(gui, KB_ESCAPE)) {
			mode = MENU;
		}
	break;
	}

	gui_end_frame(gui);
}

void menu(u32 frame_milli)
{
	const s32 w = 100;
	const s32 h = 30;
	const s32 x = screen.x / 2 - w / 2;
	s32 y = screen.y / 2 + h;
	gui_txt(gui, screen.x / 2, y + 50, 32, APP_NAME, g_white, GUI_ALIGN_CENTER);
	/* TODO: support tabbing through 'selected' buttons in gui */
	if (   (   gui_btn_txt(gui, x, y, w, h, "Solo") == BTN_PRESS
	        || key_pressed(gui, KB_1))
	    && load_maps(g_solo_maps_file_name, &maps)) {
		mode = PLAY;
		level_idx = 0;
		num_players = 1;
		strcpy(g_current_maps_file_name, g_solo_maps_file_name);
		level_init(&level, players, &maps[level_idx]);
		background_generate(&bg_effects, screen);
	}
	y -= h;
	if (   (   gui_btn_txt(gui, x, y, w, h, "Co-op") == BTN_PRESS
	        || key_pressed(gui, KB_2))
	    && load_maps(g_coop_maps_file_name, &maps)) {
		mode = PLAY;
		level_idx = 0;
		num_players = 2;
		strcpy(g_current_maps_file_name, g_coop_maps_file_name);
		level_init(&level, players, &maps[level_idx]);
		background_generate(&bg_effects, screen);
	}
	y -= h;
	if (gui_btn_txt(gui, x, y, w, h, "Edit") == BTN_PRESS || key_pressed(gui, KB_3)) {
		const struct map map_empty = { 0 };
		mode = EDIT;
		level_idx = 0;
		num_players = 1;
		array_clear(maps);
		array_append(maps, map_empty);
		g_current_maps_file_name[0] = '\0';
		file_open_dialog(g_current_maps_file_name, 128, "vson");
		editor_edit_map(&maps, level_idx);
	}
	y -= h;
	if (gui_btn_txt(gui, x, y, w, h, "Exit") == BTN_PRESS)
		quit = true;
}

void play(u32 frame_milli)
{
#ifdef DEBUG
	if (key_pressed(gui, KB_G))
		background_generate(&bg_effects, screen);
#endif // DEBUG

	array_foreach(bg_effects, struct effect, fx) {
		fx->t += (r32)frame_milli / fx->duration;
		if (fx->t > 2.f)
			fx->t = 0.f;
		if (fx->t <= 1.f) {
			const u32 sz = TILE_SIZE;
			color_t fill = fx->color;
			fill.a = 48 * fx->t + 16;
			gui_rect(gui, fx->pos.x - sz / 2, fx->pos.y - sz / 2, sz, sz, fill, g_nocolor);
		} else if (fx->t <= 2.f) {
			const u32 sz = TILE_SIZE;
			color_t fill = fx->color;
			fill.a = 48 * (2.f - fx->t) + 16;
			gui_rect(gui, fx->pos.x - sz / 2, fx->pos.y - sz / 2, sz, sz, fill, g_nocolor);
		}
	}

	{
		const s32 x = offset.x + level.map.dim.x * TILE_SIZE / 2;
		const s32 y = offset.y + level.map.dim.y * TILE_SIZE;
		char buf[16];
		snprintf(buf, 16, "Level %u", level_idx + 1);
		gui_txt(gui, x, y + 10, 20, buf, g_white, GUI_ALIGN_CENTER);
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

#if 0
	if (!level.complete) {
		gui_style_push(gui, btn, g_gui_style_invis.btn);
		if (gui_btn_img(gui, offset.x + level.map.dim.x * TILE_SIZE / 2 - 15,
		                offset.y - 50, 30, 30, "reset.png", IMG_CENTERED) == BTN_PRESS)
			level_init(&level, players, &maps[level_idx]);
		gui_style_pop(gui);
	}
#endif

	if (!level.complete) {
		for (s32 i = 0; i < level.map.dim.y; ++i) {
			const s32 y = offset.y + i * TILE_SIZE;
			for (s32 j = 0; j < level.map.dim.x; ++j) {
				const s32 x = offset.x + j * TILE_SIZE;
				const enum tile_type type = level.map.tiles[i][j].type;
				switch (type) {
				case TILE_BLANK:
				case TILE_ACTOR:
				case TILE_CLONE:
				break;
				case TILE_HALL:
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
					gui_circ(gui, x + TILE_SIZE / 2, y + TILE_SIZE / 2, TILE_SIZE / 8,
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

	if (!level.complete) {
		for (u32 i = 0; i < level.num_clones; ++i) {
			const v2i pos = v2i_scale(level.clones[i], TILE_SIZE);
			render_actor(gui, offset, pos, g_tile_fills[TILE_CLONE]);
		}

		for (u32 i = 0; i < level.num_actors; ++i) {
			const struct actor *actor = &level.actors[i];
			const v2i actor_pos = v2f_to_v2i(actor->pos);
			render_actor(gui, offset, actor_pos, g_tile_fills[TILE_ACTOR]);
			for (u32 j = 0; j < actor->num_clones; ++j) {
				const v2i clone_pos
					= v2i_add(actor_pos, v2i_scale(actor->clones[j], TILE_SIZE));
				render_actor(gui, offset, clone_pos, g_tile_fills[TILE_CLONE]);
			}
		}
	}


	if (settings_panel.hidden) {
		u32 milli_consumed[ACTOR_CNT_MAX] = {0};
		move_actors(&level, players, frame_milli, milli_consumed);

		for (u32 i = 0; i < num_players; ++i) {
			struct player *player = &players[i];
			const enum action action = player_desired_action(player, gui);

			if (action == ACTION_COUNT)
				continue;

			if (action_is_solo(action)) {
				if (can_execute_solo_action(action, player, &level))
					execute_action(action, player, &level);
				else
					sound_play(&sound_error);
			} else if (num_players == 1) {
				execute_action(action, player, &level);
			} else if (all_other_players_pending_action(players, num_players, i, action)) {
				execute_action(action, player, &level);
				for (u32 j = 0; j < num_players; ++j)
					players[j].pending_action = ACTION_COUNT;
			} else if (player->pending_action == action) {
				player->pending_action = ACTION_COUNT;
			} else {
				player->pending_action = action;
			}
		}

		move_actors(&level, players, frame_milli, milli_consumed);

		{
			const s32 x = offset.x + level.map.dim.x * TILE_SIZE / 2;
			s32 y = offset.y - 40;
			for (u32 i = 0; i < num_players; ++i) {
				struct player *player = &players[i];
				if (player->pending_action != ACTION_COUNT) {
					char buf[64];
					snprintf(buf, 64, "Player %u wants to %s", i + 1,
					         action_to_string(player->pending_action));
					gui_txt(gui, x, y, 14, buf, g_tile_fills[TILE_ACTOR], GUI_ALIGN_CENTER);
					y -= 16;
				}
			}
		}

		if (key_pressed(gui, key_prev)) {
			array_clear(dissolve_effects);
			array_clear(door_effects);
			level_idx = (level_idx + array_sz(maps) - 1) % array_sz(maps);
			level_init(&level, players, &maps[level_idx]);
			background_generate(&bg_effects, screen);
		} else if (key_pressed(gui, key_next)) {
			array_clear(dissolve_effects);
			array_clear(door_effects);
			level_idx = (level_idx + 1) % array_sz(maps);
			level_init(&level, players, &maps[level_idx]);
			background_generate(&bg_effects, screen);
		}
	}

	if (!level.complete && level_complete(&level)) {
		for (s32 i = 0; i < level.map.dim.y; ++i) {
			for (s32 j = 0; j < level.map.dim.x; ++j) {
				const v2i tile = { .x = j, .y = i };
				if (level.map.tiles[i][j].type != TILE_BLANK)
					dissolve_effect_add(&dissolve_effects, offset, tile,
					                    g_tile_fills[level.map.tiles[i][j].type],
					                    LEVEL_COMPLETE_EFFECT_DURATION_MILLI);
			}
		}
		for (u32 i = 0; i < level.num_actors; ++i) {
			const struct actor *actor = &level.actors[i];
			dissolve_effect_add(&dissolve_effects, offset, actor->tile,
			                    g_tile_fills[TILE_ACTOR],
			                    LEVEL_COMPLETE_EFFECT_DURATION_MILLI);
			for (u32 j = 0; j < actor->num_clones; ++j)
				dissolve_effect_add(&dissolve_effects, offset,
				                    v2i_add(actor->tile, actor->clones[j]),
				                    g_tile_fills[TILE_CLONE],
				                    LEVEL_COMPLETE_EFFECT_DURATION_MILLI);
		}
		sound_play(&sound_success);
		level.complete = true;
		array_foreach(door_effects, struct effect2, fx)
			if (fx->t < 1.f)
				fx->t = 2.f - fx->t;
	}

	if (level.complete && array_empty(dissolve_effects)) {
		level_idx = (level_idx + 1) % array_sz(maps);
		level_init(&level, players, &maps[level_idx]);
		background_generate(&bg_effects, screen);
	}

	if (key_pressed(gui, KB_F1) && !is_key_bound(KB_F1)) {
		editor_edit_map(&maps, level_idx);
		mode = EDIT;
	}
}
