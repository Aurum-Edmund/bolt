set(BOLT_LD_SOURCES
    bolt_ld_main.cpp
)

add_executable(bolt_ld ${BOLT_LD_SOURCES})

target_include_directories(bolt_ld
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(bolt_ld
    PRIVATE
        BOLT_LD_VERSION="Stage0"
)

if (MSVC)
    target_compile_options(bolt_ld
        PRIVATE
            /W4
            /permissive-
            /EHsc-
            /GR-
    )
else ()
    target_compile_options(bolt_ld
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif ()
