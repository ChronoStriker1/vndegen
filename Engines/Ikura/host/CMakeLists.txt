cmake_minimum_required(VERSION 3.24)
project(IkuraHost C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/../../../Packages/VNDegenKit/Sources/CIkura)
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
add_library(ikura STATIC ${RUNTIME_SOURCES} ${SHARED}/vnfont.c)
target_include_directories(ikura PUBLIC ${RUNTIME} ${RUNTIME}/include ${SHARED})
target_compile_options(ikura PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function)
target_compile_definitions(ikura PRIVATE _GNU_SOURCE)
target_link_libraries(ikura PUBLIC vnvideo m pthread)

add_executable(ikrun ikrun.c)
target_link_libraries(ikrun PRIVATE ikura)
target_compile_definitions(ikrun PRIVATE IK_FONTS_DIR="${SHARED}/fonts")

find_package(SDL2 CONFIG)
if(SDL2_FOUND)
  add_executable(ikplay ikplay.c)
  target_link_libraries(ikplay PRIVATE ikura SDL2::SDL2)
  target_compile_definitions(ikplay PRIVATE IK_FONTS_DIR="${SHARED}/fonts")
endif()
