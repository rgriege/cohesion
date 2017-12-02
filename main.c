#define VIOLET_IMPLEMENTATION
#include "violet/all.h"

#define FPS_CAP 30

int main(int argc, char *const argv[]) {
	gui_t *gui;
	b32 quit = false;
	
	gui = gui_create(0, 0, 800, 600, "ldjam", WINDOW_CENTERED);
	if (!gui)
		return 1;

	while (!quit && gui_begin_frame(gui)) {
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
