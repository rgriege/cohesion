CC = gcc
INC = /usr/include
CCFLAGS = -std=gnu99 -g -g3 -DDEBUG -Darray_size_t=u32 -Wall -Werror -Wno-missing-braces -I. -I$(INC)/ -I$(INC)/SDL2/
# CCFLAGS = -std=gnu99 -DNDEBUG -Darray_size_t=u32 -Wall -Werror -Wno-missing-braces -I. -I$(INC)/ -I$(INC)/SDL2/
LFLAGS = -lGL -lGLEW -lm -lSDL2 -lSDL2_mixer -ldl
SOURCES := main.c audio.h audio_stub.h action.h config.h constants.h disk.h editor.h history.h key.h play.h settings.h
SOUNDS_DESKTOP := $(wildcard *.aiff)
SOUNDS_WEB = $(subst aiff,mp3,$(SOUNDS_DESKTOP))
RESOURCES_CORE := $(wildcard *.png) maps.vson Roboto.ttf
RESOURCES_DESKTOP = $(SOUNDS_DESKTOP) $(RESOURCES_CORE)
RESOURCES_WEB = $(SOUNDS_WEB) $(RESOURCES_CORE)

cohesion: $(SOURCES) $(RESOURCES_DESKTOP)
	$(CC) $(CCFLAGS) -o cohesion main.c $(LFLAGS)

%.mp3: %.aiff
	sox $< $@

index.html: $(SOURCES) $(RESOURCES_WEB)
	emcc main.c -O2 -I. -I$(INC)/SDL2/ -DFMATH_NO_SSE -DDMATH_NO_SSE -s WASM=1 --shell-file html_template/shell_minimal.html -o cohesion.html -s USE_SDL=2 $(foreach R, $(RESOURCES_WEB), --preload-file $(R))
	mv cohesion.html index.html

cohesion.7z: index.html
	7z a cohesion.7z index.html cohesion.js cohesion.wasm cohesion.data

.PHONY: clean
clean:
	rm -f cohesion
	rm -f index.html cohesion.js cohesion.wasm cohesion.data
	rm -f cohesion.7z
	rm -f $(SOUNDS_WEB)
