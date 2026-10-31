# Linux test host for VNDegen's RGSS runtime: builds the runtime from its SwiftPM target with a static Ruby, and the tools that run
# games headless. Not part of the app. It links the Ruby that `scripts/rgss-ruby.sh build/rgss linux` builds.
cmake_minimum_required(VERSION 3.24)
project(RGSSHost C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(SHARED ${RUNTIME}/../Shared)
set(PACKAGE ${RUNTIME}/../../Packages/VNDegenKit/Sources/CRGSS)
if(NOT RUBY_DIR)
  set(RUBY_DIR ${RUNTIME}/../../build/rgss/linux)
endif()
if(NOT EXISTS ${RUBY_DIR}/lib/libruby.a)
  message(FATAL_ERROR "No Ruby in ${RUBY_DIR}: run scripts/rgss-ruby.sh build/rgss linux")
endif()

# The runtime: the CRGSS target's sources, which the app builds too.
file(GLOB RUNTIME_SOURCES ${PACKAGE}/core/*.c)
add_library(rgss STATIC ${RUNTIME_SOURCES} ${SHARED}/vnfont.c ${SHARED}/vnmidi.c)
target_include_directories(rgss PUBLIC ${PACKAGE}/core ${PACKAGE}/include ${SHARED} ${RUBY_DIR}/headers)
target_compile_definitions(rgss PUBLIC RGSS_FONTS_DIR="${SHARED}/fonts")
target_compile_options(rgss PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
target_link_libraries(rgss PUBLIC ${RUBY_DIR}/lib/libruby.a z m pthread dl crypt)

add_executable(rgssrun tools/rgssrun.c)
target_link_libraries(rgssrun PRIVATE rgss)
