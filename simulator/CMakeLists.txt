cmake_minimum_required(VERSION 3.20)

project(rotator_simulator C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(LVGL_DIR "${PROJECT_ROOT}/.pio/libdeps/esp32p4-release/lvgl" CACHE PATH "Pinned LVGL source directory")
set(ARDUINOJSON_DIR "${PROJECT_ROOT}/.pio/libdeps/esp32p4-release/ArduinoJson/src" CACHE PATH "ArduinoJson include directory")
set(SIMULATOR_DEPENDENCY_ROOT "" CACHE PATH "Optional SDL2 toolchain root")

if(NOT EXISTS "${LVGL_DIR}/lvgl.h")
  message(FATAL_ERROR "LVGL dependency not found. Run firmware build once: pio run")
endif()

if(SIMULATOR_DEPENDENCY_ROOT)
  find_path(SDL2_INCLUDE_DIR SDL2/SDL.h PATHS "${SIMULATOR_DEPENDENCY_ROOT}/include" NO_DEFAULT_PATH)
  find_library(SDL2_LIBRARY SDL2 PATHS "${SIMULATOR_DEPENDENCY_ROOT}/lib" NO_DEFAULT_PATH)
  find_file(SDL2_DLL SDL2.dll PATHS "${SIMULATOR_DEPENDENCY_ROOT}/bin" NO_DEFAULT_PATH)
else()
find_path(SDL2_INCLUDE_DIR SDL2/SDL.h
  PATHS
    "${SIMULATOR_DEPENDENCY_ROOT}/include"
    "C:/msys64/mingw64/include"
    "C:/msys64/ucrt64/include"
)
find_library(SDL2_LIBRARY SDL2
  PATHS
    "${SIMULATOR_DEPENDENCY_ROOT}/lib"
    "C:/msys64/mingw64/lib"
    "C:/msys64/ucrt64/lib"
)
find_file(SDL2_DLL SDL2.dll
  PATHS
    "${SIMULATOR_DEPENDENCY_ROOT}/bin"
    "C:/msys64/mingw64/bin"
    "C:/msys64/ucrt64/bin"
)
endif()

if(NOT SDL2_INCLUDE_DIR OR NOT SDL2_LIBRARY)
  message(FATAL_ERROR "SDL2 not found. Install MSYS2 mingw-w64 SDL2 or adjust simulator/CMakeLists.txt paths.")
endif()

file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS
  "${LVGL_DIR}/src/*.c"
)

file(GLOB UI_SCREEN_SOURCES CONFIGURE_DEPENDS
  "${PROJECT_ROOT}/src/ui/screens/*.cpp"
)

find_package(Git REQUIRED)
file(GLOB_RECURSE SIM_IDENTITY_INPUTS CONFIGURE_DEPENDS
  "${PROJECT_ROOT}/src/*.h" "${PROJECT_ROOT}/src/*.cpp" "${PROJECT_ROOT}/src/*.c"
  "${PROJECT_ROOT}/lib/*.h" "${PROJECT_ROOT}/lib/*.c" "${PROJECT_ROOT}/lib/*.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/*.in" "${CMAKE_CURRENT_LIST_DIR}/*.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SIM_IDENTITY_INPUTS})
execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD WORKING_DIRECTORY "${PROJECT_ROOT}"
  OUTPUT_VARIABLE SIM_BUILD_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${GIT_EXECUTABLE}" diff --quiet HEAD -- src simulator lib
  WORKING_DIRECTORY "${PROJECT_ROOT}" RESULT_VARIABLE SIM_DIRTY_RESULT)
set(SIM_BUILD_DIRTY false)
if(NOT SIM_DIRTY_RESULT EQUAL 0)
  set(SIM_BUILD_DIRTY true)
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" ls-files --others --exclude-standard -- src simulator lib
  WORKING_DIRECTORY "${PROJECT_ROOT}" OUTPUT_VARIABLE SIM_UNTRACKED)
if(NOT SIM_UNTRACKED STREQUAL "")
  set(SIM_BUILD_DIRTY true)
endif()
file(STRINGS "${PROJECT_ROOT}/src/config.h" SIM_VERSION_LINE REGEX "^#define FW_VERSION")
string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" SIM_BUILD_VERSION "${SIM_VERSION_LINE}")
configure_file(build_identity.h.in generated/build_identity.h @ONLY)

add_executable(rotator_simulator
  main.cpp
  sim_stubs.cpp
  "${PROJECT_ROOT}/src/control/program_executor.cpp"
  "${PROJECT_ROOT}/src/control/control.cpp"
  "${PROJECT_ROOT}/src/control/modes/continuous.cpp"
  "${PROJECT_ROOT}/src/control/modes/pulse.cpp"
  "${PROJECT_ROOT}/src/control/modes/step_mode.cpp"
  "${PROJECT_ROOT}/src/control/modes/jog.cpp"
  "${PROJECT_ROOT}/src/motor/motor.cpp"
  "${PROJECT_ROOT}/src/app_state.cpp"
  "${PROJECT_ROOT}/src/ui/theme.cpp"
  "${PROJECT_ROOT}/src/ui/fonts/rotator_digits_104.c"
  "${PROJECT_ROOT}/src/ui/screens.cpp"
  ${UI_SCREEN_SOURCES}
  ${LVGL_SOURCES}
)

add_executable(rotator_usb_mirror
  usb_mirror_viewer.cpp
)

target_compile_definitions(rotator_simulator PRIVATE
  ROTATOR_SIMULATOR=1
  LV_CONF_INCLUDE_SIMPLE=1
  LV_USE_SNAPSHOT=1
  SDL_MAIN_HANDLED=1
  DEBUG_BUILD=1
)

target_compile_definitions(rotator_usb_mirror PRIVATE
  SDL_MAIN_HANDLED=1
)

target_include_directories(rotator_simulator PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
  "${CMAKE_CURRENT_LIST_DIR}/stubs"
  "${PROJECT_ROOT}/src"
  "${PROJECT_ROOT}/lib"
  "${LVGL_DIR}"
  "${ARDUINOJSON_DIR}"
  "${SDL2_INCLUDE_DIR}"
)

target_include_directories(rotator_usb_mirror PRIVATE
  "${PROJECT_ROOT}"
  "${SDL2_INCLUDE_DIR}"
)

target_compile_options(rotator_simulator PRIVATE
  -Wall
  -Wextra
  -Werror=return-type
  -Wno-unused-parameter
  -Wno-missing-field-initializers
)

target_compile_options(rotator_usb_mirror PRIVATE
  -Wall
  -Wno-unused-parameter
)

target_link_libraries(rotator_simulator PRIVATE
  "${SDL2_LIBRARY}"
)

target_link_libraries(rotator_usb_mirror PRIVATE
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
  target_link_libraries(rotator_usb_mirror PRIVATE
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
  add_custom_command(TARGET rotator_usb_mirror POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${SDL2_DLL}"
      "$<TARGET_FILE_DIR:rotator_usb_mirror>/SDL2.dll"
  )
endif()
