#define VIOLET_IMPLEMENTATION
#include "violet/all.h"

#define MAP_DIM_MAX 32
#define TILE_SIZE 20
#define WALK_SPEED 40
#define CLONE_CNT_MAX 32
#define FPS_CAP 30
#define ALLOW_ROTATION

enum tile_type {
	TILE_BLANK,
	TILE_WALL,
	TILE_PLAYER,
	TILE_CLONE,
	TILE_DOOR,
};

struct tile {
	enum tile_type type;
};

struct map {
	v2i dim;
	struct tile tiles[MAP_DIM_MAX][MAP_DIM_MAX];
};

struct level {
	struct map map;
	v2i player_start;
	v2i clones[CLONE_CNT_MAX];
	u32 num_clones;
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

static
b32 load_maps(struct map **maps)
{
	b32 success = false;
	u32 n;
	FILE *fp;

	fp = fopen("maps.vson", "r");
	if (!fp)
		return false;

	if (!vson_read_u32(fp, "maps", &n))
		goto out;

	for (u32 i = 0; i < n; ++i) {
		struct map map;
		char tiles[MAP_DIM_MAX * MAP_DIM_MAX];
		if (!vson_read_s32(fp, "width", &map.dim.x))
			goto out;
		if (!vson_read_s32(fp, "height", &map.dim.y))
			goto out;
		if (   !vson_read_str(fp, "tiles", tiles, MAP_DIM_MAX * MAP_DIM_MAX)
		    || strlen(tiles) != map.dim.x * map.dim.y)
			goto out;
		for (s32 i = 0; i < map.dim.y; ++i)
			for (s32 j = 0; j < map.dim.x; ++j)
				map.tiles[i][j].type = tiles[i * map.dim.x + j] - '0';
		array_append(*maps, map);
	}
	success = true;

out:
	if (!success)
		array_clear(*maps);
	return success;
}

static
void level_init(struct level *level, const struct map *map)
{
	level->map = *map;
	level->num_clones = 0;
	for (u32 i = 0; i < map->dim.y; ++i) {
		for (u32 j = 0; j < map->dim.x; ++j) {
			switch(map->tiles[i][j].type) {
			case TILE_BLANK:
			case TILE_WALL:
			case TILE_DOOR:
			break;
			case TILE_PLAYER:
				level->player_start = (v2i){ .x = j, .y = i };
			break;
			case TILE_CLONE:
				level->clones[level->num_clones++] = (v2i){ .x = j, .y = i };
			break;
			}
		}
	}
}

static
void player_init(struct player *player, const struct level *level)
{
	player->tile = level->player_start;
	player->pos = v2i_to_v2f(v2i_scale(player->tile, TILE_SIZE));
	player->dir = DIR_NONE;
	player->num_clones = 0;
}

static
b32 tile_walkable(const struct map *map, s32 x, s32 y)
{
	return    x >= 0 && x < map->dim.x
	       && y >= 0 && y < map->dim.y
	       && map->tiles[y][x].type != TILE_WALL;
}

static
b32 tile_walkablev(const struct map *map, v2i tile)
{
	return tile_walkable(map, tile.x, tile.y);
}

static
b32 tile_walkable_for_player(const struct map *map, s32 x, s32 y,
                             const struct player *player)
{
	if (!tile_walkable(map, x, y))
		return false;
	for (u32 i = 0; i < player->num_clones; ++i)
		if (!tile_walkable(map, x + player->clones[i].x, y + player->clones[i].y))
			return false;
	return true;
}

static
void render_player(gui_t *gui, v2i offset, v2i pos, color_t color)
{
	gui_rect(gui, offset.x + 2 + pos.x, offset.y + 2 + pos.y, TILE_SIZE - 4, TILE_SIZE - 4, color, g_nocolor);
}

static
s32 manhattan_dist(v2i v0, v2i v1)
{
	return abs(v0.x - v1.x) + abs(v0.y - v1.y);
}

static
void player_entered_tile(struct level *level, v2i tile, struct player *player)
{
	for (u32 j = 0; j < level->num_clones; ) {
		if (manhattan_dist(level->clones[j], tile) == 1) {
			player->clones[player->num_clones++] = v2i_sub(level->clones[j], tile);
			level->clones[j] = level->clones[--level->num_clones];
		} else {
			++j;
		}
	}

clones:
	for (u32 i = 0; i < player->num_clones; ++i) {
		const v2i player_clone = v2i_add(tile, player->clones[i]);
		for (u32 j = 0; j < level->num_clones; ) {
			if (manhattan_dist(level->clones[j], player_clone) == 1) {
				player->clones[player->num_clones++] = v2i_sub(level->clones[j], tile);
				level->clones[j] = level->clones[--level->num_clones];
				goto clones;
			} else {
				++j;
			}
		}
	}
}

int main(int argc, char *const argv[]) {
	gui_t *gui;
	b32 quit = false;
	u32 level_idx = 0;
	struct level level;
	struct player player;
	u32 frame_milli = 0;
	array(struct map) maps;

	log_add_std(LOG_STDOUT);
	
	gui = gui_create(0, 0, 800, 600, "ldjam", WINDOW_CENTERED);
	if (!gui)
		return 1;

	maps = array_create();
	if (!maps)
		goto err_maps_create;

	if (!load_maps(&maps) || array_empty(maps))
		goto err_maps_load;

	level_init(&level, &maps[level_idx]);
	player_init(&player, &level);

	while (!quit && gui_begin_frame(gui)) {

		v2i offset;
		{
			const v2i map_dim = v2i_scale(level.map.dim, TILE_SIZE);
			v2i screen;
			gui_dim(gui, &screen.x, &screen.y);
			offset = v2i_scale_inv(v2i_sub(screen, map_dim), 2);
		}

		for (u32 i = 0; i < level.map.dim.y; ++i) {
			const s32 y = offset.y + i * TILE_SIZE;
			for (u32 j = 0; j < level.map.dim.x; ++j) {
				const s32 x = offset.x + j * TILE_SIZE;
				switch (level.map.tiles[i][j].type) {
				case TILE_BLANK:
				case TILE_PLAYER:
				case TILE_CLONE:
					gui_rect(gui, x, y, TILE_SIZE, TILE_SIZE, g_grey128, g_black);
				break;
				case TILE_WALL:
					gui_rect(gui, x, y, TILE_SIZE, TILE_SIZE, g_black, g_grey128);
				break;
				case TILE_DOOR:
					gui_rect(gui, x, y, TILE_SIZE, TILE_SIZE, g_yellow, g_black);
				break;
				}
			}
		}

		for (u32 i = 0; i < level.num_clones; ++i)
			render_player(gui, offset, v2i_scale(level.clones[i], TILE_SIZE), g_lightblue);

		switch (player.dir) {
		case DIR_NONE:
			if (   key_down(gui, KB_W)
			    && tile_walkable_for_player(&level.map, player.tile.x, player.tile.y + 1, &player)) {
				player.dir = DIR_UP;
				++player.tile.y;
			} else if (   key_down(gui, KB_S)
			           && tile_walkable_for_player(&level.map, player.tile.x, player.tile.y - 1, &player)) {
				player.dir = DIR_DOWN;
				--player.tile.y;
			} else if (   key_down(gui, KB_A)
			           && tile_walkable_for_player(&level.map, player.tile.x - 1, player.tile.y, &player)) {
				player.dir = DIR_LEFT;
				--player.tile.x;
			} else if (   key_down(gui, KB_D)
			           && tile_walkable_for_player(&level.map, player.tile.x + 1, player.tile.y, &player)) {
				player.dir = DIR_RIGHT;
				++player.tile.x;
#ifdef ALLOW_ROTATION
			} else if (key_pressed(gui, KB_Q)) {
				/* ccw */
				b32 can_rotate = true;
				for (u32 i = 0; i < player.num_clones; ++i)
					if (!tile_walkablev(&level.map, v2i_add(player.tile, v2i_lperp(player.clones[i]))))
						can_rotate = false;
				if (can_rotate)
					for (u32 i = 0; i < player.num_clones; ++i)
						player.clones[i] = v2i_lperp(player.clones[i]);
			} else if (key_pressed(gui, KB_E)) {
				/* cw */
				b32 can_rotate = true;
				for (u32 i = 0; i < player.num_clones; ++i)
					if (!tile_walkablev(&level.map, v2i_add(player.tile, v2i_rperp(player.clones[i]))))
						can_rotate = false;
				if (can_rotate)
					for (u32 i = 0; i < player.num_clones; ++i)
						player.clones[i] = v2i_rperp(player.clones[i]);
#endif // ALLOW_ROTATION
			}
		break;
		case DIR_UP:
			player.pos.y += WALK_SPEED * frame_milli / 1000.f;
			if ((s32)player.pos.y >= player.tile.y * TILE_SIZE) {
				player_entered_tile(&level, player.tile, &player);
				if (   key_down(gui, KB_W)
				    && tile_walkable_for_player(&level.map, player.tile.x, player.tile.y + 1, &player)) {
					++player.tile.y;
				} else {
					player.pos.y = player.tile.y * TILE_SIZE;
					player.dir = DIR_NONE;
				}
			}
		break;
		case DIR_DOWN:
			player.pos.y -= WALK_SPEED * frame_milli / 1000.f;
			if ((s32)player.pos.y <= player.tile.y * TILE_SIZE) {
				player_entered_tile(&level, player.tile, &player);
				if (   key_down(gui, KB_S)
				    && tile_walkable_for_player(&level.map, player.tile.x, player.tile.y - 1, &player)) {
					--player.tile.y;
				} else {
					player.pos.y = player.tile.y * TILE_SIZE;
					player.dir = DIR_NONE;
				}
			}
		break;
		case DIR_LEFT:
			player.pos.x -= WALK_SPEED * frame_milli / 1000.f;
			if ((s32)player.pos.x <= player.tile.x * TILE_SIZE) {
				player_entered_tile(&level, player.tile, &player);
				if (   key_down(gui, KB_A)
				    && tile_walkable_for_player(&level.map, player.tile.x - 1, player.tile.y, &player)) {
					--player.tile.x;
				} else {
					player.pos.x = player.tile.x * TILE_SIZE;
					player.dir = DIR_NONE;
				}
			}
		break;
		case DIR_RIGHT:
			player.pos.x += WALK_SPEED * frame_milli / 1000.f;
			if ((s32)player.pos.x >= player.tile.x * TILE_SIZE) {
				player_entered_tile(&level, player.tile, &player);
				if (   key_down(gui, KB_D)
				    && tile_walkable_for_player(&level.map, player.tile.x + 1, player.tile.y, &player)) {
					++player.tile.x;
				} else {
					player.pos.x = player.tile.x * TILE_SIZE;
					player.dir = DIR_NONE;
				}
			}
		break;
		}

		render_player(gui, offset, v2f_to_v2i(player.pos), g_orange);
		for (u32 i = 0; i < player.num_clones; ++i)
			render_player(gui, offset, v2i_add(v2f_to_v2i(player.pos), v2i_scale(player.clones[i], TILE_SIZE)), g_lightblue);

		if (   level.map.tiles[player.tile.y][player.tile.x].type == TILE_DOOR
		    && player.dir == DIR_NONE) {
			level_idx = (level_idx + 1) % array_sz(maps);
			level_init(&level, &maps[level_idx]);
			player_init(&player, &level);
		}

		if (key_pressed(gui, KB_R)) {
			level_init(&level, &maps[level_idx]);
			player_init(&player, &level);
		}

		quit = key_down(gui, KB_ESCAPE);

		gui_end_frame(gui);
		{
			frame_milli = time_diff_milli(gui_frame_start(gui), time_current());
			if (frame_milli < 1000.f / FPS_CAP) {
				time_sleep_milli((u32)(1000.f / FPS_CAP) - frame_milli);
				frame_milli = time_diff_milli(gui_frame_start(gui), time_current());
			}
		}
	}

err_maps_load:
	array_destroy(maps);
err_maps_create:
	gui_destroy(gui);
	return 0;
}
