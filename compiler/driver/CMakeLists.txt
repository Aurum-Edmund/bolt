add_library(bolt_driver_support STATIC
    import_bundle_writer.cpp
)

target_include_directories(bolt_driver_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(bolt_driver_support
    PUBLIC
        bolt_hir
)

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
        bolt_driver_support
        bolt_front_end
        bolt_hir
        bolt_mir
)

if (MSVC)
    target_compile_options(boltcc
        PRIVATE
            /W4
            /permissive-
            /EHsc-
            /GR-
    )
    target_link_libraries(boltcc
        PRIVATE
            ucrtd
            vcruntimed
    )
else ()
    target_compile_options(boltcc
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif ()



