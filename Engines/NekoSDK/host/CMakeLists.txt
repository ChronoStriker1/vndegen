# Linux test host for VNDegen's NekoSDK runtime. Builds the runtime from its SwiftPM target's sources
# (Packages/VNDegenKit/Sources/CNekoSDK) and the tools that run games headless or in a window. Not part of the app.
cmake_minimum_required(VERSION 3.24)
project(NekoSDKHost CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/../../../Packages/VNDegenKit/Sources/CNekoSDK)
set(SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared)
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)

file(GLOB RUNTIME_SOURCES ${RUNTIME}/core/*.cpp ${RUNTIME}/core/*.c)
add_library(nekosdk STATIC ${RUNTIME_SOURCES} ${SHARED}/vnfont.c)
target_include_directories(nekosdk PUBLIC ${RUNTIME}/core ${RUNTIME}/include ${SHARED})
target_compile_definitions(nekosdk PUBLIC NEKO_FONTS_DIR="${SHARED}/fonts")
target_link_libraries(nekosdk PUBLIC ZLIB::ZLIB PkgConfig::FREETYPE pthread)
# Tools write PNGs with stb_image_write, from the Wolf runtime's copy of stb.
target_include_directories(nekosdk PUBLIC ${RUNTIME}/../CWolfRPG/third_party)

foreach(tool nekocheck nekorun)
  add_executable(${tool} tools/${tool}.cpp)
  target_link_libraries(${tool} PRIVATE nekosdk)
endforeach()
# A window and speakers for playing by hand. The runtime never uses SDL.
find_package(SDL3 CONFIG)
if(SDL3_FOUND)
  add_executable(nekoplay tools/nekoplay.cpp)
  target_link_libraries(nekoplay PRIVATE nekosdk SDL3::SDL3)
endif()
