set(BOLT_UNIT_TEST_SOURCES
    unit/frontend/LexerTest.cpp
    unit/frontend/ParserTest.cpp
    unit/high_level_ir/BinderTest.cpp
    unit/high_level_ir/ImportResolverTest.cpp
    unit/high_level_ir/ModuleLocatorTest.cpp
    unit/driver/CommandLineParserTest.cpp
    unit/driver/ImportBundleWriterTest.cpp
    unit/middle_ir/LoweringTest.cpp
    unit/middle_ir/MirCanonicalTest.cpp
    unit/middle_ir/LiveEnforcementTest.cpp
    unit/middle_ir/ControlFlowGraphTest.cpp
    unit/runtime/RuntimeHelpersTest.cpp
    unit/linker/LinkerCommandLineTest.cpp
    unit/linker/LinkerInvocationTest.cpp
)

add_executable(bolt_unit_tests ${BOLT_UNIT_TEST_SOURCES})

target_link_libraries(bolt_unit_tests
    PRIVATE
        gtest_main
        bolt_driver_support
        bolt_front_end
        bolt_hir
        bolt_mir
        bolt_mir_passes
        bolt_runtime
        bolt_linker_cli
)

target_include_directories(bolt_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/compiler/linker
        ${CMAKE_SOURCE_DIR}/compiler/frontend
        ${CMAKE_SOURCE_DIR}/compiler/high_level_ir
        ${CMAKE_SOURCE_DIR}/compiler/middle_ir
        ${CMAKE_SOURCE_DIR}/compiler/driver
        ${CMAKE_SOURCE_DIR}/runtime
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
