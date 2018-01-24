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

static const color_t g_sky        = { .r=0xa9, .g=0xdf, .b=0xf7, .a=0xff };
static const color_t g_grass      = { .r=0x83, .g=0xae, .b=0x31, .a=0xff };
static const color_t g_grass_dark = { .r=0x69, .g=0x8c, .b=0x27, .a=0xff };
static const color_t g_stone      = { .r=0xe6, .g=0xe6, .b=0xe6, .a=0xff };
static const color_t g_stone_dark = { .r=0x80, .g=0x80, .b=0x80, .a=0xff };
static const color_t g_door       = { .r=0xff, .g=0xe6, .b=0x80, .a=0xff };
static const color_t g_door_dark  = { .r=0x8d, .g=0x71, .b=0x00, .a=0xff };

static const color_t g_tile_fills[] = {
	gi_nocolor,
	gi_nocolor,
	{ .r=0x83, .g=0xae, .b=0x31, .a=0xff },
	{ .r=0xe5, .g=0x7f, .b=0x73, .a=0xff },
	{ .r=0x26, .g=0xce, .b=0xd9, .a=0xff },
	{ .r=0xff, .g=0xe6, .b=0x80, .a=0xff },
	{ .r=0x7a, .g=0x26, .b=0xd9, .a=0xff },
};

static const gui_key_t key_next = KB_N;
static const gui_key_t key_prev = KB_P;
