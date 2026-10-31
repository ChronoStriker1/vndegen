# Linux test host for VNDegen's YU-RIS runtime. Builds the runtime from its SwiftPM target's sources
# (Packages/VNDegenKit/Sources/CYuRIS) with the shared font code, and the tools that run games headless or in an SDL
# window. Not part of the app.
cmake_minimum_required(VERSION 3.24)
project(YuRISHost C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/../../../Packages/VNDegenKit/Sources/CYuRIS)
set(SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared)

find_package(ZLIB REQUIRED)
file(GLOB RUNTIME_SOURCES ${RUNTIME}/*.c)
add_library(yuris STATIC ${RUNTIME_SOURCES} ${SHARED}/vnfont.c)
target_include_directories(yuris PUBLIC ${RUNTIME} ${RUNTIME}/include ${SHARED})
target_compile_options(yuris PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare)
target_link_libraries(yuris PUBLIC ZLIB::ZLIB m)

add_executable(yrrun yrrun.c)
target_link_libraries(yrrun PRIVATE yuris)
target_compile_definitions(yrrun PRIVATE YR_FONTS_DIR="${SHARED}/fonts")

find_package(SDL2 CONFIG)
if(SDL2_FOUND AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/yrplay.c)
  add_executable(yrplay yrplay.c)
  target_link_libraries(yrplay PRIVATE yuris SDL2::SDL2)
  target_compile_definitions(yrplay PRIVATE YR_FONTS_DIR="${SHARED}/fonts")
endif()
