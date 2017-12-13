CC = gcc
INC = /usr/include
CCFLAGS = -std=gnu99 -g -g3 -DDEBUG -Darray_size_t=u32 -Wall -Werror -Wno-missing-braces -I. -I$(INC)/ -I$(INC)/SDL2/
# CCFLAGS = -std=gnu99 -DNDEBUG -Darray_size_t=u32 -Wall -Werror -Wno-missing-braces -I. -I$(INC)/ -I$(INC)/SDL2/
LFLAGS = -lGL -lGLEW -lm -lSDL2 -lSDL2_mixer -ldl
HEADERS := audio.h action.h config.h constants.h disk.h editor.h history.h key.h settings.h types.h
SOURCES := action.c audio.c disk.c editor.c history.c key.c main.c settings.c
OBJECTS := $(SOURCES:c=o)
SOUNDS_DESKTOP := $(wildcard *.aiff)
SOUNDS_WEB = $(SOUNDS_DESKTOP:aiff=mp3)
RESOURCES_CORE := $(wildcard *.png) maps.vson Roboto.ttf
RESOURCES_DESKTOP = $(SOUNDS_DESKTOP) $(RESOURCES_CORE)
RESOURCES_WEB = $(SOUNDS_WEB) $(RESOURCES_CORE)

cohesion: $(OBJECTS) $(RESOURCES_DESKTOP)
	$(CC) $(CCFLAGS) -o cohesion $(OBJECTS) $(LFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CCFLAGS) -c $< -o $@

%.mp3: %.aiff
	sox $< $@

index.html: $(HEADERS) $(SOURCES) $(RESOURCES_WEB)
	emcc main.c -O2 -I. -I$(INC)/SDL2/ -DFMATH_NO_SSE -DDMATH_NO_SSE -s WASM=1 --shell-file html_template/shell_minimal.html -o cohesion.html -s USE_SDL=2 $(foreach R, $(RESOURCES_WEB), --preload-file $(R))
	mv cohesion.html index.html

cohesion.7z: index.html
	7z a cohesion.7z index.html cohesion.js cohesion.wasm cohesion.data

.PHONY: clean
clean:
	rm -f *.o
	rm -f cohesion
	rm -f index.html cohesion.js cohesion.wasm cohesion.data
	rm -f cohesion.7z
	rm -f $(SOUNDS_WEB)
