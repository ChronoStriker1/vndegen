cmake_minimum_required(VERSION 3.24)
project(ExHibitHost C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/../../../Packages/VNDegenKit/Sources/CEXHIBIT)

file(GLOB RUNTIME_SOURCES ${RUNTIME}/*.c)
add_library(exhibit STATIC ${RUNTIME_SOURCES})
target_include_directories(exhibit PUBLIC ${RUNTIME} ${RUNTIME}/include)
target_compile_options(exhibit PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function)
target_compile_definitions(exhibit PRIVATE _GNU_SOURCE)
target_link_libraries(exhibit PUBLIC m pthread)

add_executable(exrun exrun.c)
target_link_libraries(exrun PRIVATE exhibit)
