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
add_library(wolfrender STATIC src/render.cpp)
target_link_libraries(wolfrender PUBLIC wolfdata SDL3::SDL3 SDL3_ttf::SDL3_ttf)

add_executable(wolfstat tools/wolfstat.cpp)
target_link_libraries(wolfstat PRIVATE wolfdata)

add_executable(wolfunpack tools/wolfunpack.cpp)
target_link_libraries(wolfunpack PRIVATE wolfdata)

add_executable(wolfplay tools/wolfplay.cpp)
target_link_libraries(wolfplay PRIVATE wolfrender)
