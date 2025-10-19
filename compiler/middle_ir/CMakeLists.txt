set(BOLT_MIR_SOURCES
    module.cpp
    builder.cpp
    printer.cpp
    lowering.cpp
    verifier.cpp
    canonical.cpp
)

add_library(bolt_mir STATIC ${BOLT_MIR_SOURCES})

target_include_directories(bolt_mir
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../high_level_ir
        ${CMAKE_SOURCE_DIR}/compiler/frontend
)

if (MSVC)
    target_compile_options(bolt_mir PRIVATE /W4 /permissive- /EHsc- /GR-)
else()
    target_compile_options(bolt_mir PRIVATE -Wall -Wextra -Wpedantic)
endif()
