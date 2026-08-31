cmake_minimum_required(VERSION 3.22)

project(Sayri)

# SDL sources must be placed here (see README)
add_subdirectory(SDL)

# ---- SDL2_image ----
# Use the bundled STB backend for PNG/JPEG (no external
# libpng/libjpeg needed). Disable the heavier formats so their
# third-party deps are not pulled in.
set(SDL2IMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_JXL  OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_TIF  OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_WEBP OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_SVG  OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL_image)

# ---- SDL2_ttf ----
# Uses vendored freetype (fetched as a submodule). Harfbuzz is
# not required by Sayri.
set(SDL2TTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL_ttf)

# Our application (compiles the C sources into libmain.so)
add_subdirectory(src)
