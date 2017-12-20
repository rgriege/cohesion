enum tile_type {
	TILE_BLANK,
	TILE_WALL,
	TILE_HALL,
	TILE_ACTOR,
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
	u32 actor_controlled_by_player[ACTOR_CNT_MAX];
};

enum dir {
	DIR_NONE,
	DIR_UP,
	DIR_DOWN,
	DIR_LEFT,
	DIR_RIGHT,
};

struct actor {
	v2i tile;
	v2f pos;
	enum dir dir;
	v2i clones[CLONE_CNT_MAX];
	u32 num_clones;
};

struct level {
	struct map map;
	struct actor actors[ACTOR_CNT_MAX];
	u32 num_actors;
	v2i clones[CLONE_CNT_MAX];
	u32 num_clones;
	b32 complete;
};

struct history_event
{
	enum action action;
	u32 num_clones;
};

struct history
{
	struct history_event events[HISTORY_EVENT_MAX];
	u32 begin;
	u32 end;
};

struct player {
	const gui_key_t *key_bindings;
	struct actor *actors[ACTOR_CNT_MAX];
	u32 num_actors;
	enum action last_action;
	u32 action_repeat_timer;
	enum action pending_action;
	struct history history;
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
