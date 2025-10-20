add_library(bolt_linker_cli STATIC
    cli_options.cpp
    linker_invocation.cpp
)

target_include_directories(bolt_linker_cli
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if (MSVC)
    target_compile_options(bolt_linker_cli
        PRIVATE
            /W4
            /permissive-
            /EHsc-
            /GR-
    )
else ()
    target_compile_options(bolt_linker_cli
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif ()

add_executable(bolt_ld
    bolt_ld_main.cpp
)

target_link_libraries(bolt_ld
    PRIVATE
        bolt_linker_cli
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
