#include "runtime.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

namespace
{
    constexpr std::size_t kBufferSize = 64;
}

TEST(RuntimeHelpersTest, MemoryCopyTransfersAllBytes)
{
    std::array<std::uint8_t, kBufferSize> source{};
    std::array<std::uint8_t, kBufferSize> destination{};

    for (std::size_t index = 0; index < source.size(); ++index)
    {
        source[index] = static_cast<std::uint8_t>(index * 3U);
        destination[index] = 0xCCU;
    }

    void* returned = bolt_memory_copy(destination.data(), source.data(), source.size());
    EXPECT_EQ(destination.data(), returned);
    EXPECT_EQ(0, std::memcmp(destination.data(), source.data(), source.size()));
}

TEST(RuntimeHelpersTest, MemoryCopyHandlesZeroLength)
{
    std::array<std::uint8_t, kBufferSize> source{};
    std::array<std::uint8_t, kBufferSize> destination{};

    std::fill(source.begin(), source.end(), 0xABU);
    std::fill(destination.begin(), destination.end(), 0xCDU);

    void* returned = bolt_memory_copy(destination.data(), source.data(), 0);
    EXPECT_EQ(destination.data(), returned);
    EXPECT_EQ(0xCDU, destination.front());
    EXPECT_EQ(0xCDU, destination.back());
}

TEST(RuntimeHelpersTest, MemoryFillWritesRequestedByte)
{
    std::array<std::uint8_t, kBufferSize> buffer{};
    std::fill(buffer.begin(), buffer.end(), 0x00U);

    void* returned = bolt_memory_fill(buffer.data(), 0x7F, buffer.size());
    EXPECT_EQ(buffer.data(), returned);

    for (std::uint8_t byte : buffer)
    {
        EXPECT_EQ(0x7FU, byte);
    }
}

TEST(RuntimeHelpersTest, MemoryFillHandlesZeroLength)
{
    std::array<std::uint8_t, kBufferSize> buffer{};
    std::fill(buffer.begin(), buffer.end(), 0x2AU);

    void* returned = bolt_memory_fill(buffer.data(), 0x11, 0);
    EXPECT_EQ(buffer.data(), returned);
    EXPECT_EQ(0x2AU, buffer.front());
    EXPECT_EQ(0x2AU, buffer.back());
}
