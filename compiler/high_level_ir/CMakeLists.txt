set(BOLT_HIR_SOURCES
    binder.cpp
    module.cpp
)

add_library(bolt_hir STATIC ${BOLT_HIR_SOURCES})

target_include_directories(bolt_hir
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/compiler/frontend
)

target_compile_features(bolt_hir PUBLIC cxx_std_17)

if (MSVC)
    target_compile_options(bolt_hir
        PRIVATE
            /W4
            /permissive-
            /EHsc-
            /GR-
    )
else ()
    target_compile_options(bolt_hir
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif ()

