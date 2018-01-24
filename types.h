enum tile_type {
	TILE_BLANK,
	TILE_WALL,
	TILE_HALL,
	TILE_ACTOR,
	TILE_CLONE,
	TILE_DOOR,
	TILE_CLONE2,
};
#define TILE_CNT (TILE_CLONE2 + 1)

struct tile {
	enum tile_type type;
	color_t active_color;
	r32 t;
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

struct clone {
	v2i pos;
	b32 required;
};

enum dir {
	DIR_NONE,
	DIR_UP,
	DIR_DOWN,
	DIR_LEFT,
	DIR_RIGHT,
};

struct actor {
	u32 player;
	v2i tile;
	v2f pos;
	enum dir dir;
	enum dir facing;
	u32 anim_milli;
	struct clone clones[CLONE_CNT_MAX];
	u32 num_clones;
};

struct level {
	struct map map;
	struct actor actors[ACTOR_CNT_MAX];
	u32 num_actors;
	struct clone clones[CLONE_CNT_MAX];
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
