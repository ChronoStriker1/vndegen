cmake_minimum_required(VERSION 3.24)
project(BrunsHost C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/../../../Packages/VNDegenKit/Sources/CBRUNS)

file(GLOB RUNTIME_SOURCES ${RUNTIME}/*.c)
add_library(bruns STATIC ${RUNTIME_SOURCES})
target_include_directories(bruns PUBLIC ${RUNTIME} ${RUNTIME}/include)
target_compile_options(bruns PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function)
target_compile_definitions(bruns PRIVATE _GNU_SOURCE)
target_link_libraries(bruns PUBLIC m pthread z)

add_executable(brrun brrun.c)
target_link_libraries(brrun PRIVATE bruns)
