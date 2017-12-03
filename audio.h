struct sound {
	Mix_Chunk *chunk;
	int channel;
};

struct music {
	Mix_Music *handle;
};

b32 audio_init();
void audio_destroy();
b32 sound_init(struct sound *sound, const char *file);
void sound_play(struct sound *sound);
void sound_destroy(struct sound *sound);
b32 music_init(struct music *music, const char *file);
void music_play(struct music *music);
void music_destroy(struct music *music);



/* Implementation */

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
	sound->channel = Mix_PlayChannel(-1, sound->chunk, 0);
}

void sound_destroy(struct sound *sound)
{
	Mix_FreeChunk(sound->chunk);
}

b32 music_init(struct music *music, const char *file)
{
	music->handle = Mix_LoadMUS(file);
	return music->handle != NULL;
}

void music_play(struct music *music)
{
	Mix_FadeInMusic(music->handle, -1, 1000);
}

void music_destroy(struct music *music)
{
	Mix_FreeMusic(music->handle);
}
