# Linux test host for VNDegen's System 3 runtime. Builds the runtime from its SwiftPM target's sources
# (Packages/VNDegenKit/Sources/CSystem3) with the shared font and MIDI code, and the tools that run games headless or
# in an SDL window. Not part of the app.
cmake_minimum_required(VERSION 3.24)
project(System3Host C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/../../../Packages/VNDegenKit/Sources/CSystem3)
set(SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared)

file(GLOB RUNTIME_SOURCES ${RUNTIME}/*.c)
add_library(system3 STATIC ${RUNTIME_SOURCES} ${SHARED}/vnfont.c ${SHARED}/vnmidi.c)
target_include_directories(system3 PUBLIC ${RUNTIME} ${RUNTIME}/include ${SHARED})
target_compile_options(system3 PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare)
target_link_libraries(system3 PUBLIC m pthread)

add_executable(s3run s3run.c)
target_link_libraries(s3run PRIVATE system3)
target_compile_definitions(s3run PRIVATE S3_FONTS_DIR="${SHARED}/fonts" S3_SOUNDFONT="${SHARED}/soundfonts/FluidR3Mono_GM.sf3")

add_executable(cgdump cgdump.c)
target_link_libraries(cgdump PRIVATE system3)

find_package(SDL2 CONFIG)
if(SDL2_FOUND)
  add_executable(s3play s3play.c)
  target_link_libraries(s3play PRIVATE system3 SDL2::SDL2)
  target_compile_definitions(s3play PRIVATE S3_FONTS_DIR="${SHARED}/fonts" S3_SOUNDFONT="${SHARED}/soundfonts/FluidR3Mono_GM.sf3")
endif()
