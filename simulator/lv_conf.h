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

// Development: fail immediately on invalid widget API calls instead of hiding them.
#include <stdlib.h>
#undef LV_USE_CHECK_OBJ_CLASSTYPE
#undef LV_USE_CHECK_OBJ_VALIDITY
#define LV_USE_CHECK_OBJ_CLASSTYPE 0 // Upstream 9.6.0 arc class checks reference undefined obj_class
#define LV_USE_CHECK_OBJ_VALIDITY 1
#define LV_CHECK_ARG_ASSERT_ON_FAIL 1
#define LV_USE_ASSERT 1
#define LV_ASSERT_HANDLER abort();

#define LV_USE_LOG 1
#define LV_LOG_PRINTF 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_CHECK_ARG_LOG_MODE LV_CHECK_ARG_LOG_MODE_VERBOSE
