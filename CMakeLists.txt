cmake_minimum_required(VERSION 3.26)

project(Bolt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(compiler/frontend)
add_subdirectory(compiler/high_level_ir)
add_subdirectory(compiler/middle_ir)
add_subdirectory(compiler/driver)

