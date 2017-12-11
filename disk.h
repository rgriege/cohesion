b32  load_maps(array(struct map) *maps);
void save_maps(array(const struct map) maps);



/* Implementation */

const char *g_maps_file_name = "maps.vson";

b32 load_maps(array(struct map) *maps)
{
	b32 success = false;
	u32 n;
	FILE *fp;

	fp = fopen(g_maps_file_name, "r");
	if (!fp)
		return false;

	if (!vson_read_u32(fp, "maps", &n))
		goto out;

	for (u32 i = 0; i < n; ++i) {
		struct map map;
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
			for (s32 j = 0; j < map.dim.x; ++j)
				map.tiles[map.dim.y - i - 1][j].type = row[j] - '0';
		}
		array_append(*maps, map);
	}
	success = true;

out:
	if (!success)
		array_clear(*maps);
	return success;
}

void save_maps(array(const struct map) maps)
{
	FILE *fp = fopen(g_maps_file_name, "w");
	if (!fp) {
		log_error("Failed to open save file");
		return;
	}

	vson_write_u32(fp, "maps", array_sz(maps));
	array_foreach(maps, const struct map, map) {
		vson_write_str(fp, "desc", map->desc);
		vson_write_str(fp, "tip", map->tip);
		vson_write_s32(fp, "width", map->dim.x);
		vson_write_s32(fp, "height", map->dim.y);
		for (s32 i = 0; i < map->dim.y; ++i) {
			for (s32 j = 0; j < map->dim.x; ++j)
				fputc(map->tiles[map->dim.y - i - 1][j].type + '0', fp);
			fputc('\n', fp);
		}
	}
	fclose(fp);
}

