/*
 * Key bindings & audio settings
 */

gui_key_t g_key_bindings[ACTION_COUNT];

void load_settings(void);
void save_settings(void);

b32 is_key_bound(gui_key_t key);

void show_settings(gui_t *gui, gui_panel_t *panel);
void hide_settings(gui_panel_t *panel);
