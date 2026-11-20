cmake_minimum_required(VERSION 3.24)
project(ANIMHost C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/../../../Packages/VNDegenKit/Sources/CANIM)
set(SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared)

set(LIBS ${SHARED}/third_party)
file(GLOB VIDEO_SOURCES ${LIBS}/ogg/src/*.c ${LIBS}/theora/lib/*.c ${LIBS}/opus/celt/*.c ${LIBS}/opus/silk/*.c
     ${LIBS}/opus/src/*.c)
add_library(vnvideo STATIC ${SHARED}/vnvideo.c ${VIDEO_SOURCES})
target_include_directories(vnvideo PUBLIC ${SHARED} PRIVATE ${LIBS}/ogg/include ${LIBS}/theora/include ${LIBS}/opus
                           ${LIBS}/opus/include ${LIBS}/opus/celt ${LIBS}/opus/silk)
target_compile_definitions(vnvideo PRIVATE HAVE_CONFIG_H)
target_compile_options(vnvideo PRIVATE -O2 -w)
target_link_libraries(vnvideo PUBLIC m pthread)

file(GLOB RUNTIME_SOURCES ${RUNTIME}/*.c)
add_library(anim STATIC ${RUNTIME_SOURCES} ${SHARED}/vnfont.c)
target_include_directories(anim PUBLIC ${RUNTIME} ${RUNTIME}/include ${SHARED})
target_compile_options(anim PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function)
target_compile_definitions(anim PRIVATE _GNU_SOURCE)
target_link_libraries(anim PUBLIC vnvideo m pthread z)

set(EMOTE ${CMAKE_CURRENT_SOURCE_DIR}/../vnemote)
set(EMOTE_LIB ${CMAKE_CURRENT_BINARY_DIR}/emote/release/libvnemote.a)
add_custom_command(OUTPUT ${EMOTE_LIB}
  COMMAND env CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/emote cargo build --release --offline --locked
  WORKING_DIRECTORY ${EMOTE}
  DEPENDS ${EMOTE}/src/lib.rs ${EMOTE}/Cargo.toml ${EMOTE}/Cargo.lock
  USES_TERMINAL)
add_custom_target(vnemote DEPENDS ${EMOTE_LIB})
add_library(emote STATIC IMPORTED)
set_target_properties(emote PROPERTIES IMPORTED_LOCATION ${EMOTE_LIB})
add_dependencies(emote vnemote)
target_link_libraries(anim PUBLIC dl)

add_executable(animrun animrun.c)
target_link_libraries(animrun PRIVATE anim emote)
target_link_options(animrun PRIVATE -rdynamic -Wl,-u,vnemote_get_api_v1)
target_compile_definitions(animrun PRIVATE AN_FONTS_DIR="${SHARED}/fonts")

find_package(SDL2 CONFIG)
if(SDL2_FOUND)
  add_executable(animplay animplay.c)
  target_link_libraries(animplay PRIVATE anim emote SDL2::SDL2)
  target_link_options(animplay PRIVATE -rdynamic -Wl,-u,vnemote_get_api_v1)
  target_compile_definitions(animplay PRIVATE AN_FONTS_DIR="${SHARED}/fonts")
endif()
