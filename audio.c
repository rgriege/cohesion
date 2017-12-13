#include "config.h"
#include "violet/all.h"
#include "audio.h"
#ifdef AUDIO_ENABLED
#include <SDL_mixer.h>


/*
 * sound handle: Mix_Chunk
 * music handle: Mix_Music
 */

static b32 g_sound_enabled = true;
static b32 g_music_enabled = true;
static struct music g_music = { .handle = NULL };


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
	sound->handle = Mix_LoadWAV(file);
	sound->channel = -1;
	return sound->handle != NULL;
}

void sound_play(struct sound *sound)
{
	if (sound->channel >= 0)
		Mix_HaltChannel(sound->channel);
	if (g_sound_enabled)
		sound->channel = Mix_PlayChannel(-1, sound->handle, 0);
}

void sound_destroy(struct sound *sound)
{
	Mix_FreeChunk(sound->handle);
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
	g_music = *music;
	if (g_music_enabled) {
#ifdef __EMSCRIPTEN__
		/* Mix_FadeInMusic is an SDL2 feature */
		Mix_PlayMusic(music->handle, -1);
#else
		Mix_FadeInMusic(music->handle, -1, 1000);
#endif
	}
}

void music_stop(void)
{
	Mix_FadeOutMusic(1000);
	g_music.handle = NULL;
}

void music_destroy(struct music *music)
{
	Mix_FreeMusic(music->handle);
}

b32 music_enabled(void)
{
	return g_music_enabled;
}

void music_enable(void)
{
	if (!g_music_enabled) {
		g_music_enabled = true;
		if (g_music.handle)
			music_play(&g_music);
	}
}

void music_disable(void)
{
	if (g_music_enabled) {
		g_music_enabled = false;
		music_stop();
	}
}

void music_toggle(void)
{
	if (g_music_enabled)
		music_disable();
	else
		music_enable();
}
#else // AUDIO_ENABLED
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
b32  music_enabled(void) { return false; }
void music_enable(void) { return; }
void music_disable(void) { return; }
void music_toggle(void) { return; }
#endif // AUDIO_ENABLED
