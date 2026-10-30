# Linux test host for VNDegen's RGSS runtime: builds the runtime from src/ with a static Ruby, and the tools that run
# games headless. Not part of the app. RUBY_BUILD is a configured and built Ruby tree (scripts/rgss-ruby.sh linux).
cmake_minimum_required(VERSION 3.24)
project(RGSSHost C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(SHARED ${RUNTIME}/../Shared)
set(INCLUDE ${RUNTIME}/../../Packages/VNDegenKit/Sources/CRGSS/include)
if(NOT RUBY_SOURCE OR NOT RUBY_BUILD)
  message(FATAL_ERROR "Set RUBY_SOURCE and RUBY_BUILD to a Ruby source tree and its build folder")
endif()
file(GLOB RUBY_ARCH_INCLUDE LIST_DIRECTORIES true ${RUBY_BUILD}/.ext/include/*)

file(GLOB RUNTIME_SOURCES ${RUNTIME}/src/*.c)
add_library(rgss STATIC ${RUNTIME_SOURCES} ${SHARED}/vnfont.c ${SHARED}/vnmidi.c ${SHARED}/casepath.c)
target_include_directories(rgss PUBLIC ${RUNTIME}/src ${INCLUDE} ${SHARED} ${RUBY_SOURCE}/include ${RUBY_ARCH_INCLUDE})
target_compile_definitions(rgss PUBLIC RGSS_FONTS_DIR="${SHARED}/fonts")
target_compile_options(rgss PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
target_link_libraries(rgss PUBLIC
  ${RUBY_BUILD}/ext/extinit.o ${RUBY_BUILD}/enc/encinit.o
  ${RUBY_BUILD}/libruby-static.a ${RUBY_BUILD}/enc/libenc.a ${RUBY_BUILD}/enc/libtrans.a
  ${RUBY_BUILD}/ext/zlib/zlib.a ${RUBY_BUILD}/ext/stringio/stringio.a
  z m pthread dl crypt)

add_executable(rgssrun tools/rgssrun.c)
target_link_libraries(rgssrun PRIVATE rgss)
