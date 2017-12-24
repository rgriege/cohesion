#include "config.h"
#include "violet/all.h"
#include "action.h"
#include "types.h"

/* TODO: handle invalid map data */
b32 load_maps(const char *filename, array(struct map) *maps)
{
	b32 success = false;
	u32 i = 0, n = 0;
	FILE *fp;

	array_clear(*maps);

	fp = fopen(filename, "r");
	if (!fp)
		return false;

	if (!vson_read_u32(fp, "maps", &n) || n == 0)
		goto out;

	for (i = 0; i < n; ++i) {
		struct map map;
		u32 num_actors = 0;
		char row[MAP_DIM_MAX + 1];
		if (!vson_read_str(fp, "desc", map.desc, MAP_TIP_MAX))
			goto out;
		if (!vson_read_str(fp, "tip", map.tip, MAP_TIP_MAX))
			goto out;
		if (!vson_read_s32(fp, "width", &map.dim.x))
			goto out;
		if (!vson_read_s32(fp, "height", &map.dim.y))
			goto out;
		for (s32 i = 0; i < map.dim.y; ++i) {
			fgets(row, sizeof(row), fp);
			for (s32 j = 0; j < map.dim.x; ++j) {
				map.tiles[map.dim.y - i - 1][j].type = row[j] - '0';
				if (row[j] - '0' == TILE_ACTOR)
					++num_actors;
			}
		}
		fgets(row, sizeof(row), fp);
		for (u32 i = 0; i < num_actors; ++i)
			map.actor_controlled_by_player[i] = row[i] - '0';
		array_append(*maps, map);
	}
	success = true;

out:
	if (!success) {
		array_clear(*maps);
		log_error("map load error near entry %u/%u", i, n);
	}
	return success;
}

void save_maps(const char *filename, array(const struct map) maps)
{
	FILE *fp = fopen(filename, "w");
	if (!fp) {
		log_error("Failed to open save file");
		return;
	}

	vson_write_u32(fp, "maps", array_sz(maps));
	array_foreach(maps, const struct map, map) {
		u32 num_actors = 0;
		vson_write_str(fp, "desc", map->desc);
		vson_write_str(fp, "tip", map->tip);
		vson_write_s32(fp, "width", map->dim.x);
		vson_write_s32(fp, "height", map->dim.y);
		for (s32 i = 0; i < map->dim.y; ++i) {
			for (s32 j = 0; j < map->dim.x; ++j) {
				fputc(map->tiles[map->dim.y - i - 1][j].type + '0', fp);
				if (map->tiles[map->dim.y - i - 1][j].type == TILE_ACTOR)
					++num_actors;
			}
			fputc('\n', fp);
		}
		for (u32 i = 0; i < num_actors; ++i)
			fputc(map->actor_controlled_by_player[i] + '0', fp);
		fputc('\n', fp);
	}
	fclose(fp);
}

