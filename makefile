CC = gcc
INC = /usr/include
CCFLAGS = -std=gnu99 -g -g3 -DDEBUG -Darray_size_t=u32 -Wall -Werror -Wno-missing-braces -I. -I$(INC)/ -I$(INC)/SDL2/
# CCFLAGS = -std=gnu99 -DNDEBUG -Darray_size_t=u32 -Wall -Werror -Wno-missing-braces -I. -I$(INC)/ -I$(INC)/SDL2/
LFLAGS = -lGL -lGLEW -lm -lSDL2 -lSDL2_mixer -ldl
HEADERS := action.h actor.h audio.h config.h constants.h disk.h editor.h history.h key.h level.h player.h settings.h types.h
SOURCES := action.c actor.c audio.c disk.c editor.c history.c key.c level.c player.c settings.c
OBJECTS := $(SOURCES:c=o)
SOUNDS_DESKTOP := $(wildcard sounds/*.aiff)
SOUNDS_WEB = $(SOUNDS_DESKTOP:aiff=mp3)
IMAGES := \
	$(wildcard sprites/actor/*.png) \
	sprites/clone/ \
	sprites/clone2/ \
	sprites/ui/sound.png \
	sprites/ui/reset.png \
	sprites/ui/music.png \
	sprites/ui/settings.png
RESOURCES_CORE := $(IMAGES) maps.vson Roboto.ttf
RESOURCES_DESKTOP = $(SOUNDS_DESKTOP) $(RESOURCES_CORE)
RESOURCES_WEB = $(SOUNDS_WEB) $(RESOURCES_CORE)

sprites/clone/: $(wildcard sprites/actor/*.png)
	mkdir sprites/clone/
	cp sprites/actor/*.png sprites/clone/
	gimp -i -b '(colorize "sprites/clone/*.png" 191 70 0)' -b '(gimp-quit 0)'

sprites/clone2/: $(wildcard sprites/actor/*.png)
	mkdir sprites/clone2/
	cp sprites/actor/*.png sprites/clone2/
	gimp -i -b '(colorize "sprites/clone2/*.png" 290 70 0)' -b '(gimp-quit 0)'

cohesion: $(OBJECTS) main.o $(RESOURCES_DESKTOP)
	$(CC) $(CCFLAGS) -o cohesion $(OBJECTS) main.o $(LFLAGS)

analyze: $(OBJECTS) analyze.o
	$(CC) $(CCFLAGS) -o analyze $(OBJECTS) analyze.o $(LFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CCFLAGS) -c $< -o $@

%.mp3: %.aiff
	sox $< $@

index.html: $(HEADERS) $(SOURCES) $(RESOURCES_WEB)
	emcc $(SOURCES) main.c -O2 -I. -I$(INC)/SDL2/ -DFMATH_NO_SSE -DDMATH_NO_SSE -s WASM=1 --shell-file html_template/shell_minimal.html -o cohesion.html -s USE_SDL=2 --preload-file sprites/ --preload-file sounds/
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
	rm -rf sprites/clone
	rm -rf sprites/clone2
