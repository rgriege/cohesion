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

static const color_t text_color = { .r=0x22, .g=0x1f, .b=0x1f, .a=0xff };

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
color_t color_lerp(color_t a, color_t b, r32 t)
{
	return (color_t) {
		.r = (u8)(a.r * t + b.r * (1.f - t)),
		.g = (u8)(a.g * t + b.g * (1.f - t)),
		.b = (u8)(a.b * t + b.b * (1.f - t)),
		.a = (u8)(a.a * t + b.a * (1.f - t)),
	};
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
const char *dir_to_string(enum dir dir)
{
	switch (dir) {
	case DIR_NONE:
		return "none";
	case DIR_UP:
		return "up";
	case DIR_DOWN:
		return "down";
	case DIR_LEFT:
		return "left";
	case DIR_RIGHT:
		return "right";
	}
	assert(false);
	return "none";
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
			actor->facing = dir;
			actor->dir = dir;
			v2i_add_eq(&actor->tile, g_dir_vec[dir]);
		}
		sound_play(glob.sound_slide);
	break;
	case ACTION_ROTATE_CCW:
	case ACTION_ROTATE_CW:
		for (u32 i = 0; i < player->num_actors; ++i) {
			actor = player->actors[i];
			for (u32 j = 0; j < actor->num_clones; ++j)
				actor->clones[j].pos = perp(actor->clones[j].pos, action);
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

#define ANIM_FRAMES 4

struct {
	struct {
		img_t *frames[ANIM_FRAMES];
	} facing[4];
} anims[3];

static
void render_figure(gui_t *gui, v2i pos, enum dir dir, u32 stage, u32 anim_milli)
{
	const u32 total_anim_milli = 1000 * (2 * TILE_SIZE) / WALK_SPEED;
	const u32 anim_frame = (anim_milli / (total_anim_milli / ANIM_FRAMES)) % ANIM_FRAMES;
	const img_t *img = anims[stage].facing[dir-1].frames[anim_frame];
	const r32 s = TILE_SIZE / (r32)img->texture.width;
	gui_img_ex(gui, pos.x, pos.y + TILE_SIZE / 4, img, s, s, 1.f);
}

static
void render_actor(gui_t *gui, v2i pos, enum dir dir, u32 anim_milli)
{
	render_figure(gui, pos, dir, 0, anim_milli);
}

static
void render_clone(gui_t *gui, v2i pos, b32 required, enum dir dir, u32 anim_milli)
{
	render_figure(gui, pos, dir, 1 + required, anim_milli);
}

static
void door_effect_add(array(struct effect2) *door_effects, s32 x, s32 y)
{
#ifdef ASDF // SHOW_EFFECTS
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
				actor->anim_milli += walk_milli;
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
				actor->anim_milli += walk_milli;
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
				actor->anim_milli += walk_milli;
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
				actor->anim_milli += walk_milli;
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
/*struct {
	struct {
		img_t *frames[4];
	} facing[4];
} anims[2];*/
img_t imgs[3*4*3];
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

	gui_style(gui)->bg_color = g_sky;

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

	{
		static const char *names[] = { "actor", "clone", "clone2" };
		for (u32 k = 0; k < 3; ++k) {
			for (u32 i = 0; i < 4; ++i) {
				const enum dir dir = i + 1;
				for (u32 j = 0; j < 3; ++j) {
					char fname[32];
					snprintf(fname, 32, "sprites/%s/%s_%u.png", names[k], dir_to_string(dir), j);
					check(img_load(&imgs[k*12+i*3+j], fname));
				}
			}
		}
	}
	for (u32 i = 0; i < 3; ++i) {
		for (u32 j = 0; j < 4; ++j) {
			anims[i].facing[j].frames[0] = &imgs[i*12+j*3];
			anims[i].facing[j].frames[1] = &imgs[i*12+j*3+1];
			anims[i].facing[j].frames[2] = &imgs[i*12+j*3];
			anims[i].facing[j].frames[3] = &imgs[i*12+j*3+2];
		}
	}

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
	for (u32 i = 0; i < countof(imgs); ++i)
		img_destroy(&imgs[i]);
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
	gui_txt(gui, screen.x / 2, y + 50, 32, APP_NAME, text_color, GUI_ALIGN_CENTER);
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
		mode = EDIT;
		level_idx = 0;
		num_players = 1;
		array_clear(maps);
		g_current_maps_file_name[0] = '\0';
		if (   file_open_dialog(g_current_maps_file_name, 128, "vson")
		    && load_maps(g_current_maps_file_name, &maps)) {
		} else {
			g_current_maps_file_name[0] = '\0';
			const struct map map_empty = { 0 };
			array_append(maps, map_empty);
		}
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
		char buf[16];
		snprintf(buf, 16, "%u/%u", level_idx + 1, array_sz(maps));
		gui_txt(gui, 5, screen.y - 5, 20, buf, text_color, GUI_ALIGN_LEFT | GUI_ALIGN_TOP);
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
	        level.map.tip, text_color, GUI_ALIGN_CENTER);

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
				const s32 o2 = TILE_SIZE / 10;
				const s32 s2 = TILE_SIZE - 2 * o2;
				const s32 h3 = TILE_SIZE / 5;
				color_t c;
				switch (type) {
				case TILE_BLANK:
				case TILE_ACTOR:
				case TILE_CLONE:
				case TILE_CLONE2:
				break;
				case TILE_HALL:;
					c = color_lerp(level.map.tiles[i][j].active_color, g_stone,
					               level.map.tiles[i][j].t);
					gui_rect(gui, x, y, TILE_SIZE, TILE_SIZE, g_grass, g_nocolor);
					gui_rect(gui, x+o2, y+o2, s2, s2, c, g_nocolor);
					gui_line(gui, x+o2, y+o2, x+TILE_SIZE-o2, y+o2, 3, g_stone_dark);
					gui_line(gui, x+o2, y+o2, x+o2, y+TILE_SIZE-o2, 1, g_stone_dark);
					if (i == 0 || level.map.tiles[i-1][j].type == TILE_WALL)
						gui_rect(gui, x, y-h3, TILE_SIZE, h3, g_grass_dark, g_nocolor);
				break;
				case TILE_WALL:
				break;
				case TILE_DOOR:
					c = color_lerp(level.map.tiles[i][j].active_color, g_stone,
					               level.map.tiles[i][j].t);
					gui_rect(gui, x, y, TILE_SIZE, TILE_SIZE, g_door, g_nocolor);
					gui_rect(gui, x+o2, y+o2, s2, s2, c, g_nocolor);
					gui_line(gui, x+o2, y+o2, x+TILE_SIZE-o2, y+o2, 3, g_stone_dark);
					gui_line(gui, x+o2, y+o2, x+o2, y+TILE_SIZE-o2, 1, g_stone_dark);
					if (i == 0 || level.map.tiles[i-1][j].type == TILE_WALL)
						gui_rect(gui, x, y-h3, TILE_SIZE, h3, g_door_dark, g_nocolor);

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

	if (settings_panel.hidden) {
		u32 milli_consumed[ACTOR_CNT_MAX] = {0};
		move_actors(&level, players, frame_milli, milli_consumed);

		for (u32 i = 0; i < num_players; ++i) {
			struct player *player = &players[i];
			const enum action action = player_desired_action(player, gui);

			if (action == ACTION_COUNT)
				continue;

			if (action_is_solo(action)) {
				if (can_execute_solo_action(action, player, &level)) {
					execute_action(action, player, &level);
				} else {
					switch (action) {
					case ACTION_MOVE_UP:
					case ACTION_MOVE_DOWN:
					case ACTION_MOVE_LEFT:
					case ACTION_MOVE_RIGHT:
						for (u32 j = 0; j < player->num_actors; ++j)
							player->actors[j]->facing = g_action_dir[action];
					default:
					break;
					}
					sound_play(&sound_error);
				}
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

		for (u32 i = 0; i < level.num_actors; ++i)
			if (level.actors[i].dir == DIR_NONE)
				level.actors[i].anim_milli = 0;

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


	if (!level.complete) {
		const r32 dt = (r32)frame_milli / STONE_GLOW_EFFECT_DURATION_MILLI;
		for (s32 i = 0; i < level.map.dim.y; ++i)
			for (s32 j = 0; j < level.map.dim.x; ++j)
				if (   level.map.tiles[i][j].type == TILE_HALL
				    || level.map.tiles[i][j].type == TILE_DOOR)
					level.map.tiles[i][j].t = max(level.map.tiles[i][j].t - dt, 0.f);

		for (u32 i = 0; i < level.num_actors; ++i) {
			const v2i p = level.actors[i].tile;
			level.map.tiles[p.y][p.x].active_color = g_tile_fills[TILE_ACTOR];
			level.map.tiles[p.y][p.x].t = 1.f;
			for (u32 j = 0; j < level.actors[i].num_clones; ++j) {
				const v2i p2 = v2i_add(p, level.actors[i].clones[j].pos);
				level.map.tiles[p2.y][p2.x].active_color
					= level.actors[i].clones[j].required ? g_tile_fills[TILE_CLONE2] : g_tile_fills[TILE_CLONE];
				level.map.tiles[p2.y][p2.x].t = 1.f;
			}
		}

		for (u32 i = 0; i < level.num_clones; ++i) {
			const v2i pos = v2i_add(offset, v2i_scale(level.clones[i].pos, TILE_SIZE));
			render_clone(gui, pos, level.clones[i].required, DIR_DOWN, 0);
		}

		for (u32 i = 0; i < level.num_actors; ++i) {
			const struct actor *actor = &level.actors[i];
			const v2i actor_pos = v2i_add(offset, v2f_to_v2i(actor->pos));
			render_actor(gui, actor_pos, actor->facing, actor->anim_milli);
			for (u32 j = 0; j < actor->num_clones; ++j) {
				const struct clone *clone = &actor->clones[j];
				const v2i clone_pos = v2i_add(actor_pos, v2i_scale(clone->pos, TILE_SIZE));
				render_clone(gui, clone_pos, clone->required, actor->facing, actor->anim_milli);
			}
		}
	}


	if (!level.complete && level_complete(&level)) {
		for (s32 i = 0; i < level.map.dim.y; ++i) {
			for (s32 j = 0; j < level.map.dim.x; ++j) {
				const v2i tile = { .x = j, .y = i };
				if (level.map.tiles[i][j].type == TILE_HALL)
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
			for (u32 j = 0; j < actor->num_clones; ++j) {
				const struct clone *clone = &actor->clones[j];
				dissolve_effect_add(&dissolve_effects, offset,
				                    v2i_add(actor->tile, clone->pos),
				                    clone->required ? g_tile_fills[TILE_CLONE2] : g_tile_fills[TILE_CLONE],
				                    LEVEL_COMPLETE_EFFECT_DURATION_MILLI);
			}
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
