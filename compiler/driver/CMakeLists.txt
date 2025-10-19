set(BOLTCC_SOURCES
    driver_main.cpp
)

add_executable(boltcc ${BOLTCC_SOURCES})

target_include_directories(boltcc
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(boltcc
    PRIVATE
        BOLT_BUILD_PROFILE="Stage0"
)

target_link_libraries(boltcc
    PRIVATE
        bolt_front_end
        bolt_hir
        bolt_mir
        bolt_runtime
)

if (MSVC)
    target_compile_options(boltcc
        PRIVATE
            /W4
            /permissive-
            /EHsc-
            /GR-
    )
else ()
    target_compile_options(boltcc
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif ()

