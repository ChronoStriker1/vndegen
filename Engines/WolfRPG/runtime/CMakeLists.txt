cmake_minimum_required(VERSION 3.24)
project(WolfRPG CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The game data, archives and interpreter, which need no display.
add_library(wolfdata STATIC src/data.cpp src/dxa.cpp src/files.cpp src/game.cpp src/interpreter.cpp src/characters.cpp src/commands.cpp src/stb.cpp)
target_include_directories(wolfdata PUBLIC src)
if(APPLE)
  target_link_libraries(wolfdata PUBLIC "-framework CoreFoundation")
endif()

# Drawing on the CPU. Text uses FreeType; Apple builds bring their own static FreeType.
find_package(PkgConfig REQUIRED)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
add_library(wolfrender STATIC src/canvas.cpp src/images.cpp src/render.cpp)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/maprender.cpp)
  target_sources(wolfrender PRIVATE src/maprender.cpp)
endif()
target_link_libraries(wolfrender PUBLIC wolfdata PkgConfig::FREETYPE)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/text.cpp)
  target_sources(wolfrender PRIVATE src/text.cpp)
endif()

add_executable(wolfstat tools/wolfstat.cpp)
target_link_libraries(wolfstat PRIVATE wolfdata)

add_executable(wolfunpack tools/wolfunpack.cpp)
target_link_libraries(wolfunpack PRIVATE wolfdata)

add_executable(wolfplay tools/wolfplay.cpp)
target_link_libraries(wolfplay PRIVATE wolfrender)

# Parts written by separate workers; each joins the build once its file exists. A build can leave one out while it
# is being written, with -DWOLF_PARTS=audio for example.
set(WOLF_PARTS "audio;savefile" CACHE STRING "Optional parts to build when present")
foreach(part ${WOLF_PARTS})
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/${part}.cpp)
    target_sources(wolfdata PRIVATE src/${part}.cpp)
  endif()
endforeach()
# The runtime itself never uses SDL. Linux test tools may, to play sound through a device.
find_package(SDL3 CONFIG QUIET)
foreach(tool wolfaudio wolfsave)
  string(REPLACE "wolf" "" part ${tool})
  if(part STREQUAL "save")
    set(part savefile)
  endif()
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/${tool}.cpp AND part IN_LIST WOLF_PARTS)
    add_executable(${tool} tools/${tool}.cpp)
    target_link_libraries(${tool} PRIVATE wolfrender)
    if(TARGET SDL3::SDL3 AND tool STREQUAL "wolfaudio")
      target_link_libraries(${tool} PRIVATE SDL3::SDL3)
    endif()
  endif()
endforeach()
