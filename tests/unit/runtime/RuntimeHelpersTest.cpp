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

TEST(RuntimeAtomicTest, StoreAndLoad32BitValue)
{
    alignas(4) volatile std::uint32_t value = 0U;
    bolt_atomic_store_u32(&value, 42U, boltAtomicOrderRelease);
    EXPECT_EQ(42U, bolt_atomic_load_u32(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, ExchangeReturnsPrevious32BitValue)
{
    alignas(4) volatile std::uint32_t value = 19U;
    std::uint32_t previous = bolt_atomic_exchange_u32(&value, 73U, boltAtomicOrderSequentiallyConsistent);
    EXPECT_EQ(19U, previous);
    EXPECT_EQ(73U, bolt_atomic_load_u32(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, CompareExchangeUpdatesExpectedOnFailure32Bit)
{
    alignas(4) volatile std::uint32_t value = 55U;
    std::uint32_t expected = 21U;
    bool exchanged = bolt_atomic_compare_exchange_u32(&value,
        &expected,
        99U,
        boltAtomicOrderSequentiallyConsistent,
        boltAtomicOrderRelease);

    EXPECT_FALSE(exchanged);
    EXPECT_EQ(55U, expected);
    EXPECT_EQ(55U, bolt_atomic_load_u32(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, CompareExchangeStoresNewValueOnSuccess32Bit)
{
    alignas(4) volatile std::uint32_t value = 7U;
    std::uint32_t expected = 7U;
    bool exchanged = bolt_atomic_compare_exchange_u32(&value,
        &expected,
        123U,
        boltAtomicOrderAcquireRelease,
        boltAtomicOrderRelaxed);

    EXPECT_TRUE(exchanged);
    EXPECT_EQ(7U, expected);
    EXPECT_EQ(123U, bolt_atomic_load_u32(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, StoreAndLoad64BitValue)
{
    alignas(8) volatile std::uint64_t value = 0ULL;
    bolt_atomic_store_u64(&value, 0xFEDCBA9876543210ULL, boltAtomicOrderRelease);
    EXPECT_EQ(0xFEDCBA9876543210ULL, bolt_atomic_load_u64(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, ExchangeReturnsPrevious64BitValue)
{
    alignas(8) volatile std::uint64_t value = 0x1122334455667788ULL;
    std::uint64_t previous
        = bolt_atomic_exchange_u64(&value, 0x8877665544332211ULL, boltAtomicOrderSequentiallyConsistent);
    EXPECT_EQ(0x1122334455667788ULL, previous);
    EXPECT_EQ(0x8877665544332211ULL, bolt_atomic_load_u64(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, CompareExchangeBehavesFor64BitValues)
{
    alignas(8) volatile std::uint64_t value = 0x0102030405060708ULL;
    std::uint64_t expected = value;
    bool exchanged = bolt_atomic_compare_exchange_u64(&value,
        &expected,
        0x090A0B0C0D0E0F10ULL,
        boltAtomicOrderSequentiallyConsistent,
        boltAtomicOrderAcquire);

    EXPECT_TRUE(exchanged);
    EXPECT_EQ(0x0102030405060708ULL, expected);
    EXPECT_EQ(0x090A0B0C0D0E0F10ULL, bolt_atomic_load_u64(&value, boltAtomicOrderAcquire));

    expected = 0x1112131415161718ULL;
    exchanged = bolt_atomic_compare_exchange_u64(&value,
        &expected,
        0x0001020304050607ULL,
        boltAtomicOrderRelease,
        boltAtomicOrderAcquireRelease);

    EXPECT_FALSE(exchanged);
    EXPECT_EQ(0x090A0B0C0D0E0F10ULL, expected);
    EXPECT_EQ(0x090A0B0C0D0E0F10ULL, bolt_atomic_load_u64(&value, boltAtomicOrderRelaxed));
}
