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

const gui_key_t key_next = KB_N;
const gui_key_t key_prev = KB_P;
