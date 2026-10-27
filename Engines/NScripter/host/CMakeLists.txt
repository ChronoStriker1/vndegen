# Linux test host for VNDegen's NScripter runtime. Builds the runtime from its SwiftPM target's sources and runs a
# game headless, driven by commands on stdin. Not part of the app.
cmake_minimum_required(VERSION 3.20)
project(vnns_host CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/../../../Packages/VNDegenKit/Sources/CNScripter)
file(GLOB RUNTIME_SOURCES ${RUNTIME}/core/*.cpp)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FREETYPE REQUIRED freetype2)
add_executable(vnns_host main.cpp ${RUNTIME_SOURCES})
target_include_directories(vnns_host PRIVATE ${RUNTIME}/include ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(vnns_host PRIVATE ${FREETYPE_LIBRARIES} pthread)
target_compile_options(vnns_host PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
set_source_files_properties(${RUNTIME}/core/audio.cpp ${RUNTIME}/core/gfx.cpp ${RUNTIME}/core/video.cpp
  PROPERTIES COMPILE_OPTIONS "-w")
