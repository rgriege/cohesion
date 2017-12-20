#include "config.h"
#include "violet/all.h"
#include "key.h"
#include "audio.h"
#include "action.h"
#include "settings.h"

gui_key_t g_key_bindings[PLAYER_CNT_MAX][ACTION_COUNT] = {
	{
		KB_W,
		KB_S,
		KB_A,
		KB_D,
		KB_E,
		KB_Q,
		KB_Z,
		KB_C,
	},
	{
		KB_KP_8,
		KB_KP_5,
		KB_KP_4,
		KB_KP_6,
		KB_KP_9,
		KB_KP_7,
		KB_KP_1,
		KB_KP_2,
	},
};

const char *g_settings_path = "prefs.vson";
u32         g_player_to_bind = ~0;
enum action g_action_to_bind = ACTION_COUNT;

void load_settings(void)
{
	FILE *fp;
	b32 success = false;
	u32 num_players_saved, num_actions_saved;
	char buf[64];
	u32 key;
	b32 enabled;

	fp = fopen(g_settings_path, "r");
	if (!fp) {
		log_info("No settings file found");
		return;
	}

	if (!vson_read_u32(fp, "players", &num_players_saved))
		goto out;
	if (!vson_read_u32(fp, "actions", &num_actions_saved))
		goto out;
	for (u32 i = 0; i < num_players_saved; ++i) {
		for (u32 j = 0; j < num_actions_saved; ++j) {
			if (!vson_read_str(fp, "action", buf, 64))
				goto out;
			if (!(vson_read_u32(fp, "key", &key) && is_key(key)))
				goto out;
			for (u32 k = 0; k < ACTION_COUNT; ++k) {
				if (strncmp(action_to_string(k), buf, 64) == 0) {
					g_key_bindings[i][j] = key;
					break;
				}
			}
		}
	}

	if (!vson_read_b32(fp, "music_enabled", &enabled))
		goto out;
	if (enabled)
		music_enable();
	else
		music_disable();

	if (!vson_read_b32(fp, "sound_enabled", &enabled))
		goto out;
	if (enabled)
		sound_enable();
	else
		sound_disable();

	success = true;

out:
	if (!success)
		log_warn("Error loading settings");
	fclose(fp);
}

void save_settings(void)
{
	FILE *fp;

	fp = fopen(g_settings_path, "w");
	if (!fp) {
		log_error("Cannot create settings file");
		return;
	}

	vson_write_u32(fp, "players", PLAYER_CNT_MAX);
	vson_write_u32(fp, "actions", ACTION_COUNT);
	for (u32 i = 0; i < PLAYER_CNT_MAX; ++i) {
		for (u32 j = 0; j < ACTION_COUNT; ++j) {
			vson_write_str(fp, "action", action_to_string(j));
			vson_write_u32(fp, "key", g_key_bindings[i][j]);
		}
	}

	vson_write_b32(fp, "music_enabled", music_enabled());
	vson_write_b32(fp, "sound_enabled", sound_enabled());

	fclose(fp);
}

static
b32 is_key_bound_excluding(gui_key_t key, u32 player_excluded, u32 action_excluded)
{
	for (u32 i = 0; i < PLAYER_CNT_MAX; ++i)
		for (u32 j = 0; j < ACTION_COUNT; ++j)
			if (   i != player_excluded
			    && j != action_excluded
			    && g_key_bindings[i][j] == key)
			return true;
	return false;
}

b32 is_key_bound(gui_key_t key)
{
	return is_key_bound_excluding(key, ~0, ~0);
}

void show_settings(gui_t *gui, gui_panel_t *panel)
{
	static const r32 cells_key[] = { GUI_GRID_REMAINING, GUI_GRID_REMAINING };
	char buf[32];
	pgui_panel(gui, panel);
	for (u32 i = 0; i < PLAYER_CNT_MAX; ++i) {
		for (u32 j = 0; j < ACTION_COUNT; ++j) {
			pgui_row_cellsv(gui, 20, cells_key);
			sprintf(buf, "Player %u ", i + 1);
			strcat(buf, action_to_string(j));
			pgui_txt(gui, buf);
			if (g_player_to_bind == i && g_action_to_bind == j) {
				gui_style_push(gui, panel.cell_bg_color, g_orange);
				gui_style_push_u32(gui, txt.align, GUI_ALIGN_MIDCENTER);
				pgui_txt(gui, "<press a key>");
				gui_style_pop(gui);
				gui_style_pop(gui);
			} else if (g_action_to_bind != ACTION_COUNT) {
				gui_widget_interactivity_disable(gui);
				pgui_btn_txt(gui, key_to_string(g_key_bindings[i][j]));
				gui_widget_interactivity_enable(gui);
			} else if (pgui_btn_txt(gui, key_to_string(g_key_bindings[i][j])) == BTN_PRESS) {
				g_player_to_bind = i;
				g_action_to_bind = j;
			}
		}
	}

	if (g_action_to_bind != ACTION_COUNT) {
		const u8 *kb = keyboard_state(gui);
		b32 rebound = false;
		for (u32 i = 0; i < KB_COUNT; ++i) {
			if (   is_key(i)
			    && kb[i]
			    && !is_key_bound_excluding(i, g_player_to_bind, g_action_to_bind)) {
				g_key_bindings[g_player_to_bind][g_action_to_bind] = i;
				rebound = true;
				break;
			}
		}
		if (rebound) {
			g_player_to_bind = ~0;
			g_action_to_bind = ACTION_COUNT;
			save_settings();
		}
	}

	pgui_panel_finish(gui, panel);

	gui_style_push(gui, btn, g_gui_style_invis.btn);
	if (gui_btn_img(gui, panel->x + panel->width / 2, panel->y - 30, 30, 30, "reset.png", IMG_CENTERED) == BTN_PRESS)
		hide_settings(panel);
	gui_style_pop(gui);
}

void hide_settings(gui_panel_t *panel)
{
	g_player_to_bind = 0;
	g_action_to_bind = ACTION_COUNT;
	panel->hidden = true;
}
