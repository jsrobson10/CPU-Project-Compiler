cmake_minimum_required(VERSION 3.14)

project(cpu VERSION 1.0)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-g")

file(GLOB_RECURSE SOURCES src/*.c*)
add_executable(compiler ${SOURCES})

target_link_libraries(compiler PUBLIC stdc++ m pthread)

