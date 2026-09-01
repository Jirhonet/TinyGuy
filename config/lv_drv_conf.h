#ifndef LV_DRV_CONF_H
#define LV_DRV_CONF_H

#include "lv_conf.h"

#define USE_SDL 1
#define USE_SDL_GPU 0

/* Change these to the native pixel size and orientation of your AMOLED. */
#define SDL_HOR_RES 368
#define SDL_VER_RES 448
#define SDL_ZOOM 1
#define SDL_DOUBLE_BUFFERED 0
#define SDL_INCLUDE_PATH <SDL2/SDL.h>
#define SDL_DUAL_DISPLAY 0

#define USE_MONITOR 0
#define USE_MOUSE 0
#define USE_MOUSEWHEEL 0
#define USE_KEYBOARD 0
#define USE_WAYLAND 0

#endif
