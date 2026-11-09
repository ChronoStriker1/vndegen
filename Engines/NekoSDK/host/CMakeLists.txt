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
# Test Game 82's WebP pictures. Apple builds use ImageIO instead.
pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET libwebp)

file(GLOB RUNTIME_SOURCES ${RUNTIME}/core/*.cpp ${RUNTIME}/core/*.c)
add_library(nekosdk STATIC ${RUNTIME_SOURCES} ${SHARED}/vnfont.c)
target_include_directories(nekosdk PUBLIC ${RUNTIME}/core ${RUNTIME}/include ${SHARED})
target_compile_definitions(nekosdk PUBLIC NEKO_FONTS_DIR="${SHARED}/fonts")
target_link_libraries(nekosdk PUBLIC ZLIB::ZLIB PkgConfig::FREETYPE PkgConfig::WEBP pthread)
# Tools write PNGs with stb_image_write, from the Wolf runtime's copy of stb.
target_include_directories(nekosdk PUBLIC ${RUNTIME}/../CWolfRPG/third_party)

# The app's movie decoder, vnffmovie, against the system's FFmpeg, for WMV movies. Without FFmpeg the tools run the
# games with their movies left out.
pkg_check_modules(FFMPEG IMPORTED_TARGET libavformat libavcodec libswscale libswresample libavutil)
if(FFMPEG_FOUND)
  add_library(hostmovies STATIC ${SHARED}/vnffmovie.cpp)
  target_include_directories(hostmovies PUBLIC ${SHARED})
  target_link_libraries(hostmovies PUBLIC PkgConfig::FFMPEG pthread)
  target_compile_definitions(hostmovies PUBLIC NEKO_HOST_MOVIES)
  add_executable(vnffmovie_test ${SHARED}/vnffmovie_test.cpp)
  target_link_libraries(vnffmovie_test PRIVATE hostmovies)
endif()

foreach(tool nekocheck nekorun nekosave)
  add_executable(${tool} tools/${tool}.cpp)
  target_link_libraries(${tool} PRIVATE nekosdk)
  if(FFMPEG_FOUND)
    target_link_libraries(${tool} PRIVATE hostmovies)
  endif()
endforeach()
# A window and speakers for playing by hand. The runtime never uses SDL.
find_package(SDL3 CONFIG)
if(SDL3_FOUND)
  add_executable(nekoplay tools/nekoplay.cpp)
  target_link_libraries(nekoplay PRIVATE nekosdk SDL3::SDL3)
  if(FFMPEG_FOUND)
    target_link_libraries(nekoplay PRIVATE hostmovies)
  endif()
endif()
