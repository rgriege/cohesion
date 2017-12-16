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

struct level {
	struct map map;
	struct player players[PLAYER_CNT_MAX];
	u32 num_players;
	v2i clones[CLONE_CNT_MAX];
	u32 num_clones;
	b32 complete;
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
