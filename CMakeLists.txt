cmake_minimum_required(VERSION 3.12)
project(picowalker-sdl)

add_executable(picowalker-sdl
    src/main.c
    src/picowalker_structures.h
    src/picowalker_core.h
)

find_package(SDL2 CONFIG REQUIRED)

if (DEFINED VCPKG_TARGET_TRIPLET) # windows
    target_link_libraries(picowalker-sdl
        PRIVATE
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
        ${CMAKE_SOURCE_DIR}/picowalker-core.lib
    )
    target_compile_options(picowalker-sdl PRIVATE /W4)
    target_link_options(picowalker-sdl PRIVATE -mwindows)
else() # linux
    include_directories(${SDL2_INCLUDE_DIRS})
    target_compile_options(picowalker-sdl PRIVATE -Wall -Wextra -Wpedantic -std=c23)
    target_link_libraries(picowalker-sdl
        ${SDL2_LIBRARIES}
        ${CMAKE_SOURCE_DIR}/libpicowalker-core.a
    )
endif()
