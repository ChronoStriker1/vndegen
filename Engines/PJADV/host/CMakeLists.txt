cmake_minimum_required(VERSION 3.24)
project(PJADVHost C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/../../../Packages/VNDegenKit/Sources/CPJADV)
set(SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared)

file(GLOB RUNTIME_SOURCES ${RUNTIME}/*.c)
add_library(pjadv STATIC ${RUNTIME_SOURCES} ${SHARED}/vnfont.c)
target_include_directories(pjadv PUBLIC ${RUNTIME} ${RUNTIME}/include ${SHARED})
target_compile_options(pjadv PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function)
target_compile_definitions(pjadv PRIVATE _GNU_SOURCE)
target_link_libraries(pjadv PUBLIC m pthread)

add_executable(pjadvrun pjadvrun.c)
target_link_libraries(pjadvrun PRIVATE pjadv)
target_compile_definitions(pjadvrun PRIVATE PJ_FONTS_DIR="${SHARED}/fonts")

find_package(SDL2 CONFIG)
if(SDL2_FOUND)
  add_executable(pjadvplay pjadvplay.c)
  target_link_libraries(pjadvplay PRIVATE pjadv SDL2::SDL2)
  target_compile_definitions(pjadvplay PRIVATE PJ_FONTS_DIR="${SHARED}/fonts")
endif()
