cmake_minimum_required(VERSION 3.24)
project(IkuraHost C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/../../../Packages/VNDegenKit/Sources/CIkura)
set(SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared)

file(GLOB RUNTIME_SOURCES ${RUNTIME}/*.c)
add_library(ikura STATIC ${RUNTIME_SOURCES} ${SHARED}/vnfont.c)
target_include_directories(ikura PUBLIC ${RUNTIME} ${RUNTIME}/include ${SHARED})
target_compile_options(ikura PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function)
target_compile_definitions(ikura PRIVATE _GNU_SOURCE)
target_link_libraries(ikura PUBLIC m pthread)

add_executable(ikrun ikrun.c)
target_link_libraries(ikrun PRIVATE ikura)
target_compile_definitions(ikrun PRIVATE IK_FONTS_DIR="${SHARED}/fonts")

find_package(SDL2 CONFIG)
if(SDL2_FOUND)
  add_executable(ikplay ikplay.c)
  target_link_libraries(ikplay PRIVATE ikura SDL2::SDL2)
  target_compile_definitions(ikplay PRIVATE IK_FONTS_DIR="${SHARED}/fonts")
endif()
