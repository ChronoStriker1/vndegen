cmake_minimum_required(VERSION 3.24)
project(BSXHost C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/../../../Packages/VNDegenKit/Sources/CBSX)
set(SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared)

file(GLOB RUNTIME_SOURCES ${RUNTIME}/*.c)
add_library(bsx STATIC ${RUNTIME_SOURCES} ${SHARED}/vnfont.c)
target_include_directories(bsx PUBLIC ${RUNTIME} ${RUNTIME}/include ${SHARED})
target_compile_options(bsx PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function)
target_compile_definitions(bsx PRIVATE _GNU_SOURCE)
target_link_libraries(bsx PUBLIC m pthread)

add_executable(bsxrun bsxrun.c)
target_link_libraries(bsxrun PRIVATE bsx)
target_compile_definitions(bsxrun PRIVATE BX_FONTS_DIR="${SHARED}/fonts")

find_package(SDL2 CONFIG)
if(SDL2_FOUND)
  add_executable(bsxplay bsxplay.c)
  target_link_libraries(bsxplay PRIVATE bsx SDL2::SDL2)
  target_compile_definitions(bsxplay PRIVATE BX_FONTS_DIR="${SHARED}/fonts")
endif()
