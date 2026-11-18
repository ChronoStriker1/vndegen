cmake_minimum_required(VERSION 3.24)
project(IPACHost C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/../../../Packages/VNDegenKit/Sources/CIPAC)
set(SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared)

file(GLOB RUNTIME_SOURCES ${RUNTIME}/*.c)
add_library(ipac STATIC ${RUNTIME_SOURCES} ${SHARED}/vnfont.c)
target_include_directories(ipac PUBLIC ${RUNTIME} ${RUNTIME}/include ${SHARED})
target_compile_options(ipac PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function)
target_compile_definitions(ipac PRIVATE _GNU_SOURCE)
target_link_libraries(ipac PUBLIC m pthread)

add_executable(ipacrun ipacrun.c)
target_link_libraries(ipacrun PRIVATE ipac)
target_compile_definitions(ipacrun PRIVATE IP_FONTS_DIR="${SHARED}/fonts")

find_package(SDL2 CONFIG)
if(SDL2_FOUND)
  add_executable(ipacplay ipacplay.c)
  target_link_libraries(ipacplay PRIVATE ipac SDL2::SDL2)
  target_compile_definitions(ipacplay PRIVATE IP_FONTS_DIR="${SHARED}/fonts")
endif()
