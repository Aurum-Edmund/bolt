set(BOLT_MIR_PASSES_SOURCES
    live_enforcement.cpp
    control_flow_graph.cpp
    dominator_tree.cpp
    dominance_frontier.cpp
    ssa_placement.cpp
    ssa_conversion.cpp
)

add_library(bolt_mir_passes STATIC ${BOLT_MIR_PASSES_SOURCES})

target_include_directories(bolt_mir_passes
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)
