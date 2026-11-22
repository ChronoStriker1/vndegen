cmake_minimum_required(VERSION 3.24)
project(SystemAoiHost C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/../../../Packages/VNDegenKit/Sources/CSystemAoi)
set(SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared)

file(GLOB RUNTIME_SOURCES ${RUNTIME}/*.c)
add_library(systemaoi STATIC ${RUNTIME_SOURCES} ${SHARED}/vnfont.c)
target_include_directories(systemaoi PUBLIC ${RUNTIME} ${RUNTIME}/include ${SHARED})
target_compile_options(systemaoi PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function)
target_compile_definitions(systemaoi PRIVATE _GNU_SOURCE)
target_link_libraries(systemaoi PUBLIC m pthread)

add_executable(satool satool.c)
target_link_libraries(satool PRIVATE systemaoi)

add_executable(sarun sarun.c)
target_link_libraries(sarun PRIVATE systemaoi)
target_compile_definitions(sarun PRIVATE SA_FONTS_DIR="${SHARED}/fonts")
