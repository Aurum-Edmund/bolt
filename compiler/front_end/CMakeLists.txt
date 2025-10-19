set(BOLT_FRONT_END_SOURCES
    lexer.cpp
    parser.cpp
    token.cpp
)

add_library(bolt_front_end STATIC ${BOLT_FRONT_END_SOURCES})

target_include_directories(bolt_front_end
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(bolt_front_end PUBLIC cxx_std_17)

if (MSVC)
    target_compile_options(bolt_front_end
        PRIVATE
            /W4
            /permissive-
            /EHsc-
            /GR-
    )
else ()
    target_compile_options(bolt_front_end
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif ()
