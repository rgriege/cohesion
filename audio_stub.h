struct sound {};
struct music {};

b32  audio_init(void) { return true; }
void audio_destroy(void) { return; }

b32  sound_init(struct sound *sound, const char *file) { return true; }
void sound_play(struct sound *sound) { return; }
void sound_destroy(struct sound *sound) { return; }
b32  sound_enabled(void) { return false; }
void sound_enable(void) { return; }
void sound_disable(void) { return; }
void sound_toggle(void) { return; }

b32  music_init(struct music *music, const char *file) { return true; }
void music_play(struct music *music) { return; }
void music_destroy(struct music *music) { return; }

void music_stop_all(void) { return; }
