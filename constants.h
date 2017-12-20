static const v2i g_v2i_up    = { .x =  0, .y =  1 };
static const v2i g_v2i_down  = { .x =  0, .y = -1 };
static const v2i g_v2i_left  = { .x = -1, .y =  0 };
static const v2i g_v2i_right = { .x =  1, .y =  0 };

static const v2i g_dir_vec[5] = {
	{ .x =  0, .y =  0 },
	{ .x =  0, .y =  1 },
	{ .x =  0, .y = -1 },
	{ .x = -1, .y =  0 },
	{ .x =  1, .y =  0 }
};

static const enum dir g_action_dir[4] = {
	DIR_UP,
	DIR_DOWN,
	DIR_LEFT,
	DIR_RIGHT
};

static const color_t g_tile_fills[] = {
	gi_nocolor,
	gi_black,
	gi_grey128,
	gi_orange,
	gi_lightblue,
	gi_yellow,
};

static const gui_key_t key_next = KB_N;
static const gui_key_t key_prev = KB_P;
