CC = gcc
INC = /usr/include
CCFLAGS = -std=gnu99 -g -g3 -DDEBUG -Darray_size_t=u32 -Wall -Werror -Wno-missing-braces -I. -I$(INC)/ -I$(INC)/SDL2/
# CCFLAGS = -std=gnu99 -DNDEBUG -Darray_size_t=u32 -Wall -Werror -Wno-missing-braces -I. -I$(INC)/ -I$(INC)/SDL2/
LFLAGS = -lGL -lGLEW -lm -lSDL2 -lSDL2_mixer -ldl
HEADERS := action.h actor.h audio.h config.h constants.h disk.h editor.h history.h key.h level.h player.h settings.h types.h
SOURCES := action.c actor.c audio.c disk.c editor.c history.c key.c level.c player.c settings.c
OBJECTS := $(SOURCES:c=o)
SOUNDS_DESKTOP := $(wildcard data/sounds/*.aiff)
SOUNDS_WEB = $(SOUNDS_DESKTOP:aiff=mp3)
IMAGES := \
	$(wildcard data/sprites/actor/*.png) \
	data/sprites/ui/sound.png \
	data/sprites/ui/reset.png \
	data/sprites/ui/music.png \
	data/sprites/ui/settings.png
IMAGES_GENERATED := data/sprites/clone/ data/sprites/clone2/
MAPS = data/maps/maps.vson data/maps/maps_coop.vson
FONTS = data/fonts/Roboto.ttf

data/sprites/clone/: $(wildcard data/sprites/actor/*.png)
	mkdir data/sprites/clone/
	cp data/sprites/actor/*.png data/sprites/clone/
	gimp -i -b '(colorize "data/sprites/clone/*.png" 191 70 0)' -b '(gimp-quit 0)'

data/sprites/clone2/: $(wildcard data/sprites/actor/*.png)
	mkdir data/sprites/clone2/
	cp data/sprites/actor/*.png data/sprites/clone2/
	gimp -i -b '(colorize "data/sprites/clone2/*.png" 290 70 0)' -b '(gimp-quit 0)'

cohesion: $(OBJECTS) main.o $(IMAGES_GENERATED)
	$(CC) $(CCFLAGS) -o cohesion $(OBJECTS) main.o $(LFLAGS)

analyze: $(OBJECTS) analyze.o
	$(CC) $(CCFLAGS) -o analyze $(OBJECTS) analyze.o $(LFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CCFLAGS) -c $< -o $@

%.mp3: %.aiff
	sox $< $@

index.html: $(HEADERS) $(SOURCES) main.c $(IMAGES) $(IMAGES_GENERATED) $(MAPS) $(FONTS) $(SOUNDS_WEB)
	emcc $(SOURCES) main.c -O2 -I. -I$(INC)/SDL2/ -DFMATH_NO_SSE -DDMATH_NO_SSE -s WASM=1 --shell-file html_template/shell_minimal.html -o cohesion.html -s USE_SDL=2 --preload-file data/
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
	rm -rf data/sprites/clone
	rm -rf data/sprites/clone2
