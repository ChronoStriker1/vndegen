# Linux test host for VNDegen's Wolf RPG Editor runtime. Builds the runtime from its SwiftPM target's sources
# (Packages/VNDegenKit/Sources/CWolfRPG) and the tools that run games headless. Not part of the app.
cmake_minimum_required(VERSION 3.24)
project(WolfRPGHost CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/../../../Packages/VNDegenKit/Sources/CWolfRPG)
set(SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)

# The runtime: data, interpreter, drawing on the CPU, sound. Text uses FreeType here and CoreText on Apple systems.
file(GLOB RUNTIME_SOURCES ${RUNTIME}/core/*.cpp ${RUNTIME}/core/*.c)
add_library(wolfrpg STATIC ${RUNTIME_SOURCES} ${SHARED}/vnfont.c)
target_include_directories(wolfrpg PUBLIC ${RUNTIME}/core ${RUNTIME}/include ${SHARED})
target_compile_definitions(wolfrpg PUBLIC WOLF_FONTS_DIR="${SHARED}/fonts")
target_link_libraries(wolfrpg PUBLIC PkgConfig::FREETYPE pthread)

foreach(tool wolfstat wolfunpack wolfrun wolfplay wolfaudio wolfsave)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/${tool}.cpp)
    add_executable(${tool} tools/${tool}.cpp)
    target_link_libraries(${tool} PRIVATE wolfrpg)
  endif()
endforeach()
# wolfaudio can also play through an SDL3 device on Linux; the runtime never uses SDL.
find_package(SDL3 REQUIRED CONFIG)
target_link_libraries(wolfaudio PRIVATE SDL3::SDL3)
