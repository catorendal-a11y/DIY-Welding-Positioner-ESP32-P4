cmake_minimum_required(VERSION 3.20)

project(rotator_simulator C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(LVGL_DIR "${PROJECT_ROOT}/.pio/libdeps/esp32p4-release/lvgl")
set(ARDUINOJSON_DIR "${PROJECT_ROOT}/.pio/libdeps/esp32p4-release/ArduinoJson/src")

if(NOT EXISTS "${LVGL_DIR}/lvgl.h")
  message(FATAL_ERROR "LVGL dependency not found. Run firmware build once: pio run")
endif()

find_path(SDL2_INCLUDE_DIR SDL2/SDL.h
  PATHS
    "C:/msys64/mingw64/include"
    "C:/msys64/ucrt64/include"
)
find_library(SDL2_LIBRARY SDL2
  PATHS
    "C:/msys64/mingw64/lib"
    "C:/msys64/ucrt64/lib"
)
find_file(SDL2_DLL SDL2.dll
  PATHS
    "C:/msys64/mingw64/bin"
    "C:/msys64/ucrt64/bin"
)

if(NOT SDL2_INCLUDE_DIR OR NOT SDL2_LIBRARY)
  message(FATAL_ERROR "SDL2 not found. Install MSYS2 mingw-w64 SDL2 or adjust simulator/CMakeLists.txt paths.")
endif()

file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS
  "${LVGL_DIR}/src/*.c"
)

file(GLOB UI_SCREEN_SOURCES CONFIGURE_DEPENDS
  "${PROJECT_ROOT}/src/ui/screens/*.cpp"
)

add_executable(rotator_simulator
  main.cpp
  sim_stubs.cpp
  "${PROJECT_ROOT}/src/app_state.cpp"
  "${PROJECT_ROOT}/src/ui/theme.cpp"
  "${PROJECT_ROOT}/src/ui/screens.cpp"
  ${UI_SCREEN_SOURCES}
  ${LVGL_SOURCES}
)

target_compile_definitions(rotator_simulator PRIVATE
  ROTATOR_SIMULATOR=1
  LV_CONF_INCLUDE_SIMPLE=1
  SDL_MAIN_HANDLED=1
  DEBUG_BUILD=1
)

target_include_directories(rotator_simulator PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}"
  "${CMAKE_CURRENT_LIST_DIR}/stubs"
  "${PROJECT_ROOT}/src"
  "${PROJECT_ROOT}/lib"
  "${LVGL_DIR}"
  "${ARDUINOJSON_DIR}"
  "${SDL2_INCLUDE_DIR}"
)

target_compile_options(rotator_simulator PRIVATE
  -Wall
  -Wno-unused-parameter
  -Wno-unused-variable
  -Wno-unused-function
  -Wno-sign-compare
  -Wno-missing-field-initializers
)

target_link_libraries(rotator_simulator PRIVATE
  "${SDL2_LIBRARY}"
)

if(WIN32)
  target_link_libraries(rotator_simulator PRIVATE
    user32
    gdi32
    winmm
    imm32
    ole32
    oleaut32
    version
  )
endif()

if(SDL2_DLL)
  add_custom_command(TARGET rotator_simulator POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${SDL2_DLL}"
      "$<TARGET_FILE_DIR:rotator_simulator>/SDL2.dll"
  )
endif()
