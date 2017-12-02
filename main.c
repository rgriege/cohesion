#define VIOLET_IMPLEMENTATION
#include "violet/all.h"

#define MAP_DIM_MAX 20
#define TILE_SIZE 20
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

#define g_tile_blank { .type = TILE_BLANK }
#define g_tile_wall  { .type = TILE_WALL }

const struct map g_map0 = {
	.dim = { .x = 7, .y = 3 },
	.tiles = {
		{ g_tile_wall, g_tile_wall, g_tile_wall, g_tile_wall, g_tile_wall, g_tile_wall, g_tile_wall },
		{ g_tile_wall, g_tile_blank, g_tile_blank, g_tile_blank, g_tile_blank, g_tile_blank, g_tile_wall },
		{ g_tile_wall, g_tile_wall, g_tile_wall, g_tile_wall, g_tile_wall, g_tile_wall, g_tile_wall },
	},
};

int main(int argc, char *const argv[]) {
	gui_t *gui;
	b32 quit = false;
	const struct map *map = &g_map0;
	
	gui = gui_create(0, 0, 800, 600, "ldjam", WINDOW_CENTERED);
	if (!gui)
		return 1;

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

		quit = key_down(gui, KB_Q);
		gui_end_frame(gui);
		{
			const u32 frame_milli = time_diff_milli(gui_frame_start(gui), time_current());
			if (frame_milli < 1000.f / FPS_CAP)
				time_sleep_milli((u32)(1000.f / FPS_CAP) - frame_milli);
		}
	}

	gui_destroy(gui);
	return 0;
}
