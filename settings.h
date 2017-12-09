/*
 * Key bindings & audio settings
 */

static gui_key_t g_key_bindings[ACTION_COUNT] = {
	KB_W,
	KB_S,
	KB_A,
	KB_D,
	KB_Q,
	KB_E,
	KB_SPACE,
};

void load_settings(void);
void save_settings(void);
void show_settings(gui_t *gui, gui_panel_t *panel);
void hide_settings(gui_panel_t *panel);


/* Implementation */

const char *g_settings_path = "prefs.vson";
enum action g_action_to_bind = ACTION_COUNT;

void load_settings(void)
{
	FILE *fp;
	b32 success = false;
	u32 num_actions_saved;
	char buf[64];
	u32 key;
	b32 enabled;

	fp = fopen(g_settings_path, "r");
	if (!fp) {
		log_info("No settings file found");
		return;
	}

	if (!vson_read_u32(fp, "n", &num_actions_saved))
		goto out;
	for (u32 i = 0; i < num_actions_saved; ++i) {
		if (!vson_read_str(fp, "action", buf, 64))
			goto out;
		if (!(vson_read_u32(fp, "key", &key) && is_key(key)))
			goto out;
		for (u32 j = 0; j < ACTION_COUNT; ++j) {
			if (strncmp(action_to_string(i), buf, 64) == 0) {
				g_key_bindings[i] = key;
				break;
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

	vson_write_u32(fp, "n", ACTION_COUNT);
	for (u32 i = 0; i < ACTION_COUNT; ++i) {
		vson_write_str(fp, "action", action_to_string(i));
		vson_write_u32(fp, "key", g_key_bindings[i]);
	}

	vson_write_b32(fp, "music_enabled", music_enabled());
	vson_write_b32(fp, "sound_enabled", sound_enabled());

	fclose(fp);
}

static
b32 is_key_bound(gui_key_t key, u32 excluding)
{
	for (u32 i = 0; i < ACTION_COUNT; ++i)
		if (i != excluding && g_key_bindings[i] == key)
			return true;
	return false;
}

void show_settings(gui_t *gui, gui_panel_t *panel)
{
	static const r32 cells_key[] = { GUI_GRID_REMAINING, GUI_GRID_REMAINING };
	static const r32 cells_back[] = { GUI_GRID_REMAINING, 30, GUI_GRID_REMAINING };
	pgui_panel(gui, panel);
	for (u32 i = 0; i < ACTION_COUNT; ++i) {
		pgui_row_cellsv(gui, 20, cells_key);
		pgui_txt(gui, action_to_string(i));
		if (g_action_to_bind == i) {
			gui_style_push(gui, panel.cell_bg_color, g_orange);
			gui_style_push_u32(gui, txt.align, GUI_ALIGN_MIDCENTER);
			pgui_txt(gui, "<press a key>");
			gui_style_pop(gui);
			gui_style_pop(gui);
		} else if (g_action_to_bind != ACTION_COUNT) {
			gui_widget_interactivity_disable(gui);
			pgui_btn_txt(gui, key_to_string(g_key_bindings[i]));
			gui_widget_interactivity_enable(gui);
		} else if (pgui_btn_txt(gui, key_to_string(g_key_bindings[i])) == BTN_PRESS) {
			g_action_to_bind = i;
		}
	}

	if (g_action_to_bind != ACTION_COUNT) {
		const u8 *kb = keyboard_state(gui);
		b32 rebound = false;
		for (u32 i = 0; i < KB_COUNT; ++i) {
			if (is_key(i) && kb[i] && !is_key_bound(i, g_action_to_bind)) {
				g_key_bindings[g_action_to_bind] = i;
				rebound = true;
				break;
			}
		}
		if (rebound) {
			g_action_to_bind = ACTION_COUNT;
			save_settings();
		}
	}

	pgui_row_cellsv(gui, 30, cells_back);
	pgui_spacer(gui);
	gui_style_push(gui, btn, g_gui_style_invis.btn);
	if (pgui_btn_img(gui, "reset.png", IMG_CENTERED) == BTN_PRESS)
		hide_settings(panel);
	gui_style_pop(gui);
	pgui_spacer(gui);

	pgui_panel_finish(gui, panel);
}

void hide_settings(gui_panel_t *panel)
{
	g_action_to_bind = ACTION_COUNT;
	panel->hidden = true;
}
