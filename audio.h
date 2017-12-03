struct sound {
	Mix_Chunk *chunk;
	int channel;
};

b32 audio_init();
void audio_destroy();
b32 sound_init(struct sound *sound, const char *file);
void sound_play(struct sound *sound);
void sound_destroy(struct sound *sound);



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
