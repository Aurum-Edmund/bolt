set(BOLT_UNIT_TEST_SOURCES
    unit/frontend/LexerTest.cpp
    unit/frontend/ParserTest.cpp
    unit/high_level_ir/BinderTest.cpp
    unit/high_level_ir/ImportResolverTest.cpp
    unit/middle_ir/LoweringTest.cpp
    unit/middle_ir/MirCanonicalTest.cpp
)

add_executable(bolt_unit_tests ${BOLT_UNIT_TEST_SOURCES})

target_link_libraries(bolt_unit_tests
    PRIVATE
        gtest_main
        bolt_front_end
        bolt_hir
        bolt_mir
)

target_include_directories(bolt_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/compiler/frontend
        ${CMAKE_SOURCE_DIR}/compiler/high_level_ir
        ${CMAKE_SOURCE_DIR}/compiler/middle_ir
)

add_test(NAME bolt_unit_tests COMMAND bolt_unit_tests)
if (TARGET boltcc)
    set(_golden_output "${CMAKE_BINARY_DIR}/golden/blueprint_sample.mir")
    add_test(NAME golden_mir_blueprint
        COMMAND ${CMAKE_COMMAND}
            -DBOLTCC=$<TARGET_FILE:boltcc>
            -DSOURCE=${CMAKE_SOURCE_DIR}/examples/blueprint_sample.bolt
            -DGOLDEN=${CMAKE_SOURCE_DIR}/examples/golden/blueprint_sample.mir
            -DOUTPUT=${_golden_output}
            -P ${CMAKE_SOURCE_DIR}/tests/golden/RunMirGolden.cmake)
endif ()
