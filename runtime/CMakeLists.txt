set(BOLT_RUNTIME_SOURCES
    runtime.c
)

add_library(bolt_runtime STATIC ${BOLT_RUNTIME_SOURCES})

target_include_directories(bolt_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(bolt_runtime
    PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
)

if (MSVC)
    target_compile_options(bolt_runtime
        PRIVATE
            /W4
            /permissive-
    )
else ()
    target_compile_options(bolt_runtime
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif ()
