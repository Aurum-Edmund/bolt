set(BOLT_MIR_PASSES_SOURCES
    live_enforcement.cpp
)

add_library(bolt_mir_passes STATIC )

target_include_directories(bolt_mir_passes
    PUBLIC
        /..
)
