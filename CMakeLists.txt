cmake_minimum_required(VERSION 3.16)
project(tiny_guy_simulator LANGUAGES C)

if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

# Keep the simulator on LVGL 8.3, matching the intended ESP32 firmware API.
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/config/lv_conf.h" CACHE STRING "" FORCE)
set(LV_CONF_INCLUDE_SIMPLE ON CACHE BOOL "" FORCE)
set(LV_LVGL_H_INCLUDE_SIMPLE ON CACHE BOOL "" FORCE)
add_subdirectory(external/lvgl EXCLUDE_FROM_ALL)

add_library(lvgl_sdl STATIC
    external/lv_drivers/sdl/sdl.c
    external/lv_drivers/sdl/sdl_common.c
)
target_include_directories(lvgl_sdl PUBLIC
    config
    external/lv_drivers
)
target_link_libraries(lvgl_sdl PUBLIC lvgl PkgConfig::SDL2)

add_executable(tiny-guy-sim
    src/main.c
    src/ui.c
    src/components/usage_bar.c
    src/components/page_shell.c
    src/pages/agent_page.c
    src/pages/usage_page.c
    src/services/claude_usage.c
    src/services/codex_usage.c
    src/services/cursor_usage.c
)
target_include_directories(tiny-guy-sim PRIVATE config src)
target_link_libraries(tiny-guy-sim PRIVATE lvgl_sdl Threads::Threads m)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tiny-guy-sim PRIVATE -Wall -Wextra -Wpedantic)
endif()
