#include <gtest/gtest.h>

#include "command_line.hpp"

namespace
{
    TEST(CommandLineParserTest, ParsesImportRootEqualsForm)
    {
        const char* argv[] = {
            "boltcc",
            "--import-root=modules",
            "input.bolt"
        };

        bolt::CommandLineParser parser;
        auto options = parser.parse(static_cast<int>(std::size(argv)), const_cast<char**>(argv));
        ASSERT_TRUE(options.has_value());
        ASSERT_EQ(options->importRoots.size(), 1u);
        EXPECT_EQ(options->importRoots.front(), "modules");
        ASSERT_EQ(options->inputPaths.size(), 1u);
        EXPECT_EQ(options->inputPaths.front(), "input.bolt");
    }

    TEST(CommandLineParserTest, ParsesImportRootSeparateArgument)
    {
        const char* argv[] = {
            "boltcc",
            "--import-root",
            "../deps",
            "main.bolt"
        };

        bolt::CommandLineParser parser;
        auto options = parser.parse(static_cast<int>(std::size(argv)), const_cast<char**>(argv));
        ASSERT_TRUE(options.has_value());
        ASSERT_EQ(options->importRoots.size(), 1u);
        EXPECT_EQ(options->importRoots.front(), "../deps");
        ASSERT_EQ(options->inputPaths.size(), 1u);
        EXPECT_EQ(options->inputPaths.front(), "main.bolt");
    }

    TEST(CommandLineParserTest, MissingImportRootValueFails)
    {
        const char* argv[] = {
            "boltcc",
            "--import-root"
        };

        bolt::CommandLineParser parser;
        auto options = parser.parse(static_cast<int>(std::size(argv)), const_cast<char**>(argv));
        EXPECT_FALSE(options.has_value());
    }
} // namespace
