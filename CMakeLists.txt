cmake_minimum_required(VERSION 3.26)

project(Bolt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(compiler/front_end)
add_subdirectory(compiler/hir)
add_subdirectory(compiler/mir)
add_subdirectory(compiler/driver)

