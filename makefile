CC = gcc
INC = /usr/include
CCFLAGS = -std=gnu99 -g -g3 -DDEBUG -Darray_size_t=u32 -Wall -Werror -Wno-missing-braces -I. -I$(INC)/ -I$(INC)/SDL2/
# CCFLAGS = -std=gnu99 -DNDEBUG -Darray_size_t=u32 -Wall -Werror -Wno-missing-braces -I. -I$(INC)/ -I$(INC)/SDL2/
LFLAGS = -lGL -lGLEW -lm -lSDL2 -lSDL2_mixer -ldl
SOUNDS_DESKTOP := $(wildcard *.aiff)
SOUNDS_WEB = $(subst aiff,mp3,$(SOUNDS_DESKTOP))
RESOURCES_CORE := $(wildcard *.png) maps.vson Roboto.ttf
RESOURCES_DESKTOP = $(SOUNDS_DESKTOP) $(RESOURCES_CORE)
RESOURCES_WEB = $(SOUNDS_WEB) $(RESOURCES_CORE)

cohesion: main.c audio.h $(RESOURCES_DESKTOP)
	$(CC) $(CCFLAGS) -o cohesion main.c $(LFLAGS)

%.mp3: %.aiff
	sox $< $@

cohesion.html: main.c audio.h $(RESOURCES_WEB)
	emcc main.c -O2 -I. -I$(INC)/SDL2/ -DFMATH_NO_SSE -DDMATH_NO_SSE -s WASM=1 -o cohesion.html -s USE_SDL=2 $(foreach R, $(RESOURCES_WEB), --preload-file $(R))

cohesion.7z: cohesion.html
	7z a cohesion.7z cohesion.html cohesion.js cohesion.wasm cohesion.data

.PHONY: clean
clean:
	rm -f cohesion
	rm -f cohesion.html cohesion.js cohesion.wasm cohesion.data
	rm -f cohesion.7z
	rm -f $(SOUNDS_WEB)
