// PC simulator LVGL config - extends firmware config with SDL display backend

#include "../lib/lv_conf.h"

#ifndef LV_USE_SDL
#define LV_USE_SDL 1
#endif

#ifndef LV_SDL_INCLUDE_PATH
#define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
#endif

#ifndef LV_SDL_DIRECT_EXIT
#define LV_SDL_DIRECT_EXIT 1
#endif

// LVGL 9.6: choose the existing software renderer explicitly.
#define LV_SDL_AUTO_BACKEND 0
#define LV_SDL_BACKEND LV_SDL_BACKEND_SW
