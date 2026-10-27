cmake_minimum_required(VERSION 3.24)
project(WolfRPG CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The game data, archives and interpreter, which need no display.
add_library(wolfdata STATIC src/data.cpp src/dxa.cpp src/files.cpp src/game.cpp)
target_include_directories(wolfdata PUBLIC src)
if(APPLE)
  target_link_libraries(wolfdata PUBLIC "-framework CoreFoundation")
endif()

find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)
add_library(wolfrender STATIC src/images.cpp src/render.cpp)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/maprender.cpp)
  target_sources(wolfrender PRIVATE src/maprender.cpp)
endif()
target_link_libraries(wolfrender PUBLIC wolfdata SDL3::SDL3 SDL3_ttf::SDL3_ttf)

add_executable(wolfstat tools/wolfstat.cpp)
target_link_libraries(wolfstat PRIVATE wolfdata)

add_executable(wolfunpack tools/wolfunpack.cpp)
target_link_libraries(wolfunpack PRIVATE wolfdata)

add_executable(wolfplay tools/wolfplay.cpp)
target_link_libraries(wolfplay PRIVATE wolfrender)

# Parts written by separate workers; each joins the build once its file exists.
foreach(part audio savefile)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/${part}.cpp)
    target_sources(wolfdata PRIVATE src/${part}.cpp)
  endif()
endforeach()
foreach(tool wolfaudio wolfsave)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/${tool}.cpp)
    add_executable(${tool} tools/${tool}.cpp)
    target_link_libraries(${tool} PRIVATE wolfrender)
  endif()
endforeach()
