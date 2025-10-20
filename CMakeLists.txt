cmake_minimum_required(VERSION 3.26)

project(Bolt LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BOLT_ENABLE_TESTS "Build unit tests" ON)

add_subdirectory(compiler/frontend)
add_subdirectory(compiler/high_level_ir)
add_subdirectory(compiler/middle_ir)
add_subdirectory(runtime)
add_subdirectory(compiler/linker)
add_subdirectory(compiler/driver)

if (BOLT_ENABLE_TESTS)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/googletest)
    enable_testing()
    add_subdirectory(tests)
endif ()
