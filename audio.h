struct sound {
	void *handle;
	int channel;
};

struct music {
	void *handle;
};

b32  audio_init(void);
void audio_destroy(void);

b32  sound_init(struct sound *sound, const char *file);
void sound_play(struct sound *sound);
void sound_destroy(struct sound *sound);
b32  sound_enabled(void);
void sound_enable(void);
void sound_disable(void);
void sound_toggle(void);

b32  music_init(struct music *music, const char *file);
void music_play(struct music *music);
void music_stop(void);
void music_destroy(struct music *music);
b32  music_enabled(void);
void music_enable(void);
void music_disable(void);
void music_toggle(void);
