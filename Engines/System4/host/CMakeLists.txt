# Linux test host for VNDegen's System 4 runtime. Builds the runtime from its SwiftPM target's sources
# (Packages/VNDegenKit/Sources/CSystem4) and the tools that run games headless. Not part of the app.
cmake_minimum_required(VERSION 3.24)
project(System4Host CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/../../../Packages/VNDegenKit/Sources/CSystem4)
set(SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared)
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET libwebp)

file(GLOB RUNTIME_SOURCES ${RUNTIME}/core/*.cpp ${RUNTIME}/core/*.c)
add_library(system4 STATIC ${RUNTIME_SOURCES} ${SHARED}/vnfont.c)
target_include_directories(system4 PUBLIC ${RUNTIME}/core ${RUNTIME}/include ${SHARED})
target_link_libraries(system4 PUBLIC ZLIB::ZLIB PkgConfig::FREETYPE PkgConfig::WEBP pthread)
# Tools write PNGs with stb_image_write, from the Wolf runtime's copy of stb.
target_include_directories(system4 PUBLIC ${RUNTIME}/../CWolfRPG/third_party)

foreach(tool s4dis s4play s4cg s4cgcheck s4save s4movie s4fnl)
  add_executable(${tool} tools/${tool}.cpp)
  # HLL modules register themselves from static initializers, so keep every object.
  target_link_libraries(${tool} PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,system4>")
endforeach()
