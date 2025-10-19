set(BOLT_UNIT_TEST_SOURCES
    unit/frontend/LexerTest.cpp
)

add_executable(bolt_unit_tests ${BOLT_UNIT_TEST_SOURCES})

target_link_libraries(bolt_unit_tests
    PRIVATE
        gtest_main
        bolt_front_end
)

target_include_directories(bolt_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/compiler/frontend
)

add_test(NAME bolt_unit_tests COMMAND bolt_unit_tests)
