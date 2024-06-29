OBJS = main.c
OBJ_NAME = sdl_demo

all : $(OBJS)
	g++ -I. -I../.. $(OBJS) -w -lSDL2 -o $(OBJ_NAME) -fpermissive -DCUTE_SOUND_SDL_H="<SDL2/SDL.h>"
