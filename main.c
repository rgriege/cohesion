#define VIOLET_IMPLEMENTATION
#include "violet/all.h"

#define MAP_DIM_MAX 20
#define TILE_SIZE 20
#define WALK_SPEED 40
#define FPS_CAP 30

enum tile_type {
	TILE_BLANK,
	TILE_WALL,
};

struct tile {
	enum tile_type type;
};

struct map {
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
};

#define g_tile_blank { .type = TILE_BLANK }
#define g_tile_wall  { .type = TILE_WALL }

const struct map g_map0 = {
	.dim = { .x = 7, .y = 5 },
	.tiles = {
		{ g_tile_wall, g_tile_wall, g_tile_wall, g_tile_wall, g_tile_wall, g_tile_wall, g_tile_wall },
		{ g_tile_wall, g_tile_blank, g_tile_blank, g_tile_blank, g_tile_blank, g_tile_blank, g_tile_wall },
		{ g_tile_wall, g_tile_blank, g_tile_blank, g_tile_blank, g_tile_blank, g_tile_blank, g_tile_wall },
		{ g_tile_wall, g_tile_blank, g_tile_blank, g_tile_blank, g_tile_blank, g_tile_blank, g_tile_wall },
		{ g_tile_wall, g_tile_wall, g_tile_wall, g_tile_wall, g_tile_wall, g_tile_wall, g_tile_wall },
	},
};

static
b32 tile_walkable(const struct map *map, s32 x, s32 y)
{
	return    x >= 0 && x < map->dim.x
	       && y >= 0 && y < map->dim.y
	       && map->tiles[y][x].type == TILE_BLANK;
}

int main(int argc, char *const argv[]) {
	gui_t *gui;
	b32 quit = false;
	const struct map *map = &g_map0;
	struct player player = { .tile = { .x = 1, .y = 1 }, .dir = DIR_NONE, };
	u32 frame_milli = 0;

	log_add_std(LOG_STDOUT);
	
	gui = gui_create(0, 0, 800, 600, "ldjam", WINDOW_CENTERED);
	if (!gui)
		return 1;

	player.pos = v2i_to_v2f(v2i_scale(player.tile, TILE_SIZE));

	while (!quit && gui_begin_frame(gui)) {

		v2i offset;
		{
			const v2i map_dim = v2i_scale(map->dim, TILE_SIZE);
			v2i screen;
			gui_dim(gui, &screen.x, &screen.y);
			offset = v2i_scale_inv(v2i_sub(screen, map_dim), 2);
		}

		for (u32 i = 0; i < map->dim.y; ++i) {
			for (u32 j = 0; j < map->dim.x; ++j) {
				switch (map->tiles[i][j].type) {
				case TILE_BLANK:
					gui_rect(gui, offset.x + j * TILE_SIZE, offset.y + i * TILE_SIZE, TILE_SIZE, TILE_SIZE, g_grey128, g_black);
				break;
				case TILE_WALL:
					gui_rect(gui, offset.x + j * TILE_SIZE, offset.y + i * TILE_SIZE, TILE_SIZE, TILE_SIZE, g_black, g_grey128);
				break;
				}
			}
		}

		switch (player.dir) {
		case DIR_NONE:
			if (   key_down(gui, KB_W)
			    && tile_walkable(map, player.tile.x, player.tile.y + 1)) {
				player.dir = DIR_UP;
				++player.tile.y;
			} else if (   key_down(gui, KB_S)
			           && tile_walkable(map, player.tile.x, player.tile.y - 1)) {
				player.dir = DIR_DOWN;
				--player.tile.y;
			} else if (   key_down(gui, KB_A)
			           && tile_walkable(map, player.tile.x - 1, player.tile.y)) {
				player.dir = DIR_LEFT;
				--player.tile.x;
			} else if (   key_down(gui, KB_D)
			           && tile_walkable(map, player.tile.x + 1, player.tile.y)) {
				player.dir = DIR_RIGHT;
				++player.tile.x;
			}
		break;
		case DIR_UP:
			player.pos.y += WALK_SPEED * frame_milli / 1000.f;
			if ((s32)player.pos.y >= player.tile.y * TILE_SIZE) {
				if (   key_down(gui, KB_W)
				    && tile_walkable(map, player.tile.x, player.tile.y + 1)) {
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
				if (   key_down(gui, KB_S)
				    && tile_walkable(map, player.tile.x, player.tile.y - 1)) {
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
				if (   key_down(gui, KB_A)
				    && tile_walkable(map, player.tile.x - 1, player.tile.y)) {
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
				if (   key_down(gui, KB_D)
				    && tile_walkable(map, player.tile.x + 1, player.tile.y)) {
					++player.tile.x;
				} else {
					player.pos.x = player.tile.x * TILE_SIZE;
					player.dir = DIR_NONE;
				}
			}
		break;
		}

		gui_rect(gui, offset.x + 2 + player.pos.x, offset.y + 2 + player.pos.y, TILE_SIZE - 4, TILE_SIZE - 4, g_orange, g_nocolor);

		quit = key_down(gui, KB_Q);
		gui_end_frame(gui);
		{
			frame_milli = time_diff_milli(gui_frame_start(gui), time_current());
			if (frame_milli < 1000.f / FPS_CAP) {
				time_sleep_milli((u32)(1000.f / FPS_CAP) - frame_milli);
				frame_milli = time_diff_milli(gui_frame_start(gui), time_current());
			}
		}
	}

	gui_destroy(gui);
	return 0;
}
