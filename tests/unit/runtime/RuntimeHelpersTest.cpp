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

TEST(RuntimeHelpersTest, MemoryCopyHandlesOverlappingForwardRanges)
{
    std::array<std::uint8_t, kBufferSize> buffer{};
    for (std::size_t index = 0; index < buffer.size(); ++index)
    {
        buffer[index] = static_cast<std::uint8_t>(index);
    }

    void* returned = bolt_memory_copy(buffer.data() + 4, buffer.data(), 20);
    EXPECT_EQ(buffer.data() + 4, returned);

    for (std::size_t index = 0; index < 20; ++index)
    {
        EXPECT_EQ(static_cast<std::uint8_t>(index), buffer[index + 4]);
    }
}

TEST(RuntimeHelpersTest, MemoryCopyHandlesSelfCopy)
{
    std::array<std::uint8_t, kBufferSize> buffer{};
    for (std::size_t index = 0; index < buffer.size(); ++index)
    {
        buffer[index] = static_cast<std::uint8_t>((index * 7U) & 0xFFU);
    }

    void* returned = bolt_memory_copy(buffer.data(), buffer.data(), buffer.size());
    EXPECT_EQ(buffer.data(), returned);

    for (std::size_t index = 0; index < buffer.size(); ++index)
    {
        EXPECT_EQ(static_cast<std::uint8_t>((index * 7U) & 0xFFU), buffer[index]);
    }
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

TEST(RuntimeAtomicTest, StoreAndLoad8BitValue)
{
    alignas(1) volatile std::uint8_t value = 0U;
    bolt_atomic_store_u8(&value, 0x5AU, boltAtomicOrderRelease);
    EXPECT_EQ(0x5AU, bolt_atomic_load_u8(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, StoreAndLoad16BitValue)
{
    alignas(2) volatile std::uint16_t value = 0U;
    bolt_atomic_store_u16(&value, 0xBEEF, boltAtomicOrderRelease);
    EXPECT_EQ(0xBEEFU, bolt_atomic_load_u16(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, ExchangeReturnsPrevious32BitValue)
{
    alignas(4) volatile std::uint32_t value = 19U;
    std::uint32_t previous = bolt_atomic_exchange_u32(&value, 73U, boltAtomicOrderSequentiallyConsistent);
    EXPECT_EQ(19U, previous);
    EXPECT_EQ(73U, bolt_atomic_load_u32(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, FetchAddReturnsPrevious32BitValue)
{
    alignas(4) volatile std::uint32_t value = 10U;
    std::uint32_t previous = bolt_atomic_fetch_add_u32(&value, 5U, boltAtomicOrderAcquireRelease);
    EXPECT_EQ(10U, previous);
    EXPECT_EQ(15U, bolt_atomic_load_u32(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, FetchSubReturnsPrevious32BitValue)
{
    alignas(4) volatile std::uint32_t value = 40U;
    std::uint32_t previous = bolt_atomic_fetch_sub_u32(&value, 8U, boltAtomicOrderAcquireRelease);
    EXPECT_EQ(40U, previous);
    EXPECT_EQ(32U, bolt_atomic_load_u32(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, FetchBitwiseOperations32Bit)
{
    alignas(4) volatile std::uint32_t value = 0xFFFF00FFU;

    std::uint32_t previous
        = bolt_atomic_fetch_and_u32(&value, 0x0FFF0FF0U, boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0xFFFF00FFU, previous);
    EXPECT_EQ(0x0FFF00F0U, bolt_atomic_load_u32(&value, boltAtomicOrderAcquire));

    previous = bolt_atomic_fetch_or_u32(&value, 0x0000000FU, boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0x0FFF00F0U, previous);
    EXPECT_EQ(0x0FFF00FFU, bolt_atomic_load_u32(&value, boltAtomicOrderAcquire));

    previous = bolt_atomic_fetch_xor_u32(&value, 0x00000FF0U, boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0x0FFF00FFU, previous);
    EXPECT_EQ(0x0FFF0F0FU, bolt_atomic_load_u32(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, ExchangeReturnsPrevious8BitValue)
{
    alignas(1) volatile std::uint8_t value = 0x12U;
    std::uint8_t previous
        = bolt_atomic_exchange_u8(&value, 0x34U, boltAtomicOrderSequentiallyConsistent);
    EXPECT_EQ(0x12U, previous);
    EXPECT_EQ(0x34U, bolt_atomic_load_u8(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, FetchAddReturnsPrevious8BitValue)
{
    alignas(1) volatile std::uint8_t value = 0x10U;
    std::uint8_t previous
        = bolt_atomic_fetch_add_u8(&value, 0x05U, boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0x10U, previous);
    EXPECT_EQ(0x15U, bolt_atomic_load_u8(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, FetchSubReturnsPrevious8BitValue)
{
    alignas(1) volatile std::uint8_t value = 0x32U;
    std::uint8_t previous
        = bolt_atomic_fetch_sub_u8(&value, 0x10U, boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0x32U, previous);
    EXPECT_EQ(0x22U, bolt_atomic_load_u8(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, FetchBitwiseOperations8Bit)
{
    alignas(1) volatile std::uint8_t value = 0xF3U;

    std::uint8_t previous
        = bolt_atomic_fetch_and_u8(&value, 0x9CU, boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0xF3U, previous);
    EXPECT_EQ(0x90U, bolt_atomic_load_u8(&value, boltAtomicOrderAcquire));

    previous = bolt_atomic_fetch_or_u8(&value, 0x03U, boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0x90U, previous);
    EXPECT_EQ(0x93U, bolt_atomic_load_u8(&value, boltAtomicOrderAcquire));

    previous = bolt_atomic_fetch_xor_u8(&value, 0x3CU, boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0x93U, previous);
    EXPECT_EQ(0xAFU, bolt_atomic_load_u8(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, ExchangeReturnsPrevious16BitValue)
{
    alignas(2) volatile std::uint16_t value = 0x1357U;
    std::uint16_t previous
        = bolt_atomic_exchange_u16(&value, 0x2468U, boltAtomicOrderSequentiallyConsistent);
    EXPECT_EQ(0x1357U, previous);
    EXPECT_EQ(0x2468U, bolt_atomic_load_u16(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, FetchAddReturnsPrevious16BitValue)
{
    alignas(2) volatile std::uint16_t value = 0x0100U;
    std::uint16_t previous
        = bolt_atomic_fetch_add_u16(&value, 0x0020U, boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0x0100U, previous);
    EXPECT_EQ(0x0120U, bolt_atomic_load_u16(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, FetchSubReturnsPrevious16BitValue)
{
    alignas(2) volatile std::uint16_t value = 0x0200U;
    std::uint16_t previous
        = bolt_atomic_fetch_sub_u16(&value, 0x0010U, boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0x0200U, previous);
    EXPECT_EQ(0x01F0U, bolt_atomic_load_u16(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, FetchBitwiseOperations16Bit)
{
    alignas(2) volatile std::uint16_t value = 0xFF00U;

    std::uint16_t previous
        = bolt_atomic_fetch_and_u16(&value, 0x0FF0U, boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0xFF00U, previous);
    EXPECT_EQ(0x0F00U, bolt_atomic_load_u16(&value, boltAtomicOrderAcquire));

    previous = bolt_atomic_fetch_or_u16(&value, 0x000FU, boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0x0F00U, previous);
    EXPECT_EQ(0x0F0FU, bolt_atomic_load_u16(&value, boltAtomicOrderAcquire));

    previous = bolt_atomic_fetch_xor_u16(&value, 0x00F0U, boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0x0F0FU, previous);
    EXPECT_EQ(0x0FFFU, bolt_atomic_load_u16(&value, boltAtomicOrderAcquire));
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

TEST(RuntimeAtomicTest, CompareExchangeUpdatesExpectedOnFailure8Bit)
{
    alignas(1) volatile std::uint8_t value = 0x90U;
    std::uint8_t expected = 0xABU;
    bool exchanged = bolt_atomic_compare_exchange_u8(&value,
        &expected,
        0x11U,
        boltAtomicOrderSequentiallyConsistent,
        boltAtomicOrderRelease);

    EXPECT_FALSE(exchanged);
    EXPECT_EQ(0x90U, expected);
    EXPECT_EQ(0x90U, bolt_atomic_load_u8(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, CompareExchangeUpdatesExpectedOnFailure16Bit)
{
    alignas(2) volatile std::uint16_t value = 0xF00DU;
    std::uint16_t expected = 0xFACEU;
    bool exchanged = bolt_atomic_compare_exchange_u16(&value,
        &expected,
        0x0BADU,
        boltAtomicOrderSequentiallyConsistent,
        boltAtomicOrderAcquireRelease);

    EXPECT_FALSE(exchanged);
    EXPECT_EQ(0xF00DU, expected);
    EXPECT_EQ(0xF00DU, bolt_atomic_load_u16(&value, boltAtomicOrderAcquire));
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

TEST(RuntimeAtomicTest, CompareExchangeBehavesFor8BitValues)
{
    alignas(1) volatile std::uint8_t value = 0x55U;
    std::uint8_t expected = 0x55U;
    bool exchanged = bolt_atomic_compare_exchange_u8(&value,
        &expected,
        0xAAU,
        boltAtomicOrderAcquireRelease,
        boltAtomicOrderRelaxed);

    EXPECT_TRUE(exchanged);
    EXPECT_EQ(0x55U, expected);
    EXPECT_EQ(0xAAU, bolt_atomic_load_u8(&value, boltAtomicOrderAcquire));

    expected = 0x80U;
    exchanged = bolt_atomic_compare_exchange_u8(&value,
        &expected,
        0x33U,
        boltAtomicOrderRelease,
        boltAtomicOrderAcquire);

    EXPECT_FALSE(exchanged);
    EXPECT_EQ(0xAAU, expected);
    EXPECT_EQ(0xAAU, bolt_atomic_load_u8(&value, boltAtomicOrderRelaxed));
}

TEST(RuntimeAtomicTest, CompareExchangeBehavesFor16BitValues)
{
    alignas(2) volatile std::uint16_t value = 0x1234U;
    std::uint16_t expected = value;
    bool exchanged = bolt_atomic_compare_exchange_u16(&value,
        &expected,
        0x5678U,
        boltAtomicOrderAcquireRelease,
        boltAtomicOrderRelaxed);

    EXPECT_TRUE(exchanged);
    EXPECT_EQ(0x1234U, expected);
    EXPECT_EQ(0x5678U, bolt_atomic_load_u16(&value, boltAtomicOrderAcquire));

    expected = 0x9ABCU;
    exchanged = bolt_atomic_compare_exchange_u16(&value,
        &expected,
        0x0001U,
        boltAtomicOrderRelease,
        boltAtomicOrderAcquire);

    EXPECT_FALSE(exchanged);
    EXPECT_EQ(0x5678U, expected);
    EXPECT_EQ(0x5678U, bolt_atomic_load_u16(&value, boltAtomicOrderRelaxed));
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

TEST(RuntimeAtomicTest, FetchAddReturnsPrevious64BitValue)
{
    alignas(8) volatile std::uint64_t value = 0x0000000000001000ULL;
    std::uint64_t previous = bolt_atomic_fetch_add_u64(&value,
        0x0000000000000100ULL,
        boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0x0000000000001000ULL, previous);
    EXPECT_EQ(0x0000000000001100ULL, bolt_atomic_load_u64(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, FetchSubReturnsPrevious64BitValue)
{
    alignas(8) volatile std::uint64_t value = 0x0000000000002000ULL;
    std::uint64_t previous = bolt_atomic_fetch_sub_u64(&value,
        0x0000000000000010ULL,
        boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0x0000000000002000ULL, previous);
    EXPECT_EQ(0x0000000000001FF0ULL, bolt_atomic_load_u64(&value, boltAtomicOrderAcquire));
}

TEST(RuntimeAtomicTest, FetchBitwiseOperations64Bit)
{
    alignas(8) volatile std::uint64_t value = 0xFFFF0000FFFF0000ULL;

    std::uint64_t previous = bolt_atomic_fetch_and_u64(&value,
        0x0FFF0FFF0FFF0FFFULL,
        boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0xFFFF0000FFFF0000ULL, previous);
    EXPECT_EQ(0x0FFF00000FFF0000ULL, bolt_atomic_load_u64(&value, boltAtomicOrderAcquire));

    previous = bolt_atomic_fetch_or_u64(&value,
        0x00000000000000FFULL,
        boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0x0FFF00000FFF0000ULL, previous);
    EXPECT_EQ(0x0FFF00000FFF00FFULL, bolt_atomic_load_u64(&value, boltAtomicOrderAcquire));

    previous = bolt_atomic_fetch_xor_u64(&value,
        0x0000000000000FF0ULL,
        boltAtomicOrderAcquireRelease);
    EXPECT_EQ(0x0FFF00000FFF00FFULL, previous);
    EXPECT_EQ(0x0FFF00000FFF0F0FULL, bolt_atomic_load_u64(&value, boltAtomicOrderAcquire));
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
