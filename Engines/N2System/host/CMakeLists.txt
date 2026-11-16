# Linux test host for VNDegen's N2System runtime. Builds the runtime from its SwiftPM target's sources
# (Packages/VNDegenKit/Sources/CN2System) with the shared font code, and the tools that run games headless or in an
# SDL window. Not part of the app.
cmake_minimum_required(VERSION 3.24)
project(N2SystemHost C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/../../../Packages/VNDegenKit/Sources/CN2System)
set(SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared)

file(GLOB RUNTIME_SOURCES ${RUNTIME}/*.c)
add_library(n2system STATIC ${RUNTIME_SOURCES} ${SHARED}/vnfont.c)
target_include_directories(n2system PUBLIC ${RUNTIME} ${RUNTIME}/include ${SHARED})
target_compile_options(n2system PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function)
target_compile_definitions(n2system PRIVATE _GNU_SOURCE)
target_link_libraries(n2system PUBLIC m pthread z)

add_executable(n2run n2run.c)
target_link_libraries(n2run PRIVATE n2system)
target_compile_definitions(n2run PRIVATE N2_FONTS_DIR="${SHARED}/fonts")

find_package(SDL2 CONFIG)
if(SDL2_FOUND)
  add_executable(n2play n2play.c)
  target_link_libraries(n2play PRIVATE n2system SDL2::SDL2)
  target_compile_definitions(n2play PRIVATE N2_FONTS_DIR="${SHARED}/fonts")
endif()
