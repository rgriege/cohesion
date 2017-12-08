struct sound {
	Mix_Chunk *chunk;
	int channel;
};

struct music {
	Mix_Music *handle;
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
void music_destroy(struct music *music);

void music_stop_all(void);



/* Implementation */

static b32 g_sound_enabled = true;


b32 audio_init()
{
	if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 1024) == -1) {
		log_error("Unable to open audio: %s\n", SDL_GetError());
		return false;
	}

	if (Mix_AllocateChannels(16) != 16) {
		log_error("Unable to allocate channels: %s\n", SDL_GetError());
		return false;
	}

	Mix_VolumeMusic(MIX_MAX_VOLUME / 8);

	return true;
}

void audio_destroy()
{
	Mix_CloseAudio();
}

b32 sound_init(struct sound *sound, const char *file)
{
	sound->chunk = Mix_LoadWAV(file);
	sound->channel = -1;
	return sound->chunk != NULL;
}

void sound_play(struct sound *sound)
{
	if (sound->channel >= 0)
		Mix_HaltChannel(sound->channel);
	if (g_sound_enabled)
		sound->channel = Mix_PlayChannel(-1, sound->chunk, 0);
}

void sound_destroy(struct sound *sound)
{
	Mix_FreeChunk(sound->chunk);
}

b32 sound_enabled(void)
{
	return g_sound_enabled;
}

void sound_enable(void)
{
	g_sound_enabled = true;
}

void sound_disable(void)
{
	g_sound_enabled = false;
}

void sound_toggle(void)
{
	g_sound_enabled = !g_sound_enabled;
}

b32 music_init(struct music *music, const char *file)
{
#ifdef __EMSCRIPTEN__
	/* Sweet Mary Mother of Jesus why do I have to do it this way?!?! */
	SDL_RWops *ops = SDL_RWFromFile(file, "rb");
	music->handle = Mix_LoadMUS_RW(ops, 1);
#else
	music->handle = Mix_LoadMUS(file);
#endif
	return music->handle != NULL;
}

void music_play(struct music *music)
{
#ifdef __EMSCRIPTEN__
	/* Mix_FadeInMusic is an SDL2 feature */
	Mix_PlayMusic(music->handle, -1);
#else
	Mix_FadeInMusic(music->handle, -1, 1000);
#endif
}

void music_destroy(struct music *music)
{
	Mix_FreeMusic(music->handle);
}

void music_stop_all(void)
{
	Mix_FadeOutMusic(1000);
}
