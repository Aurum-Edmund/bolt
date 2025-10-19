set(BOLT_UNIT_TEST_SOURCES
    unit/frontend/LexerTest.cpp
    unit/frontend/ParserTest.cpp
    unit/high_level_ir/BinderTest.cpp
)

add_executable(bolt_unit_tests ${BOLT_UNIT_TEST_SOURCES})

target_link_libraries(bolt_unit_tests
    PRIVATE
        gtest_main
        bolt_front_end
        bolt_hir
)

target_include_directories(bolt_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/compiler/frontend
        ${CMAKE_SOURCE_DIR}/compiler/high_level_ir
)

add_test(NAME bolt_unit_tests COMMAND bolt_unit_tests)
