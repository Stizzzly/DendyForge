#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "cartridge/cartridge.hpp"

namespace
{

dendyforge::Cartridge MakeAxromCartridge(std::uint8_t prgBanks)
{
    dendyforge::INesHeader header{};
    header.prgRomBanks = prgBanks;
    header.chrRomBanks = 0;
    header.flags6 = 0x71; // mapper 7; header mirroring must be ignored

    std::vector<std::uint8_t> prgRom(
        static_cast<std::size_t>(prgBanks) * 16 * 1024);
    for (std::size_t bank = 0; bank < prgRom.size() / (32 * 1024); ++bank)
    {
        std::fill_n(prgRom.begin() + bank * 32 * 1024, 32 * 1024,
                    static_cast<std::uint8_t>(bank));
    }

    return dendyforge::Cartridge(header, std::move(prgRom), {});
}

std::uint8_t ReadCpu(dendyforge::Cartridge& cartridge, std::uint16_t address)
{
    std::uint8_t data = 0;
    REQUIRE(cartridge.CpuRead(address, data));
    return data;
}

std::uint8_t ReadPpu(dendyforge::Cartridge& cartridge, std::uint16_t address)
{
    std::uint8_t data = 0;
    REQUIRE(cartridge.PpuRead(address, data));
    return data;
}

} // namespace

TEST_CASE("Mapper 7 switches complete 32 KiB PRG banks")
{
    auto cartridge = MakeAxromCartridge(16); // 8 x 32 KiB banks

    CHECK(ReadCpu(cartridge, 0x8000) == 0);
    CHECK(ReadCpu(cartridge, 0xFFFF) == 0);

    REQUIRE(cartridge.CpuWrite(0x8000, 0x05));
    CHECK(ReadCpu(cartridge, 0x8000) == 5);
    CHECK(ReadCpu(cartridge, 0xBFFF) == 5);
    CHECK(ReadCpu(cartridge, 0xC000) == 5);
    CHECK(ReadCpu(cartridge, 0xFFFF) == 5);

    REQUIRE(cartridge.CpuWrite(0xFFFF, 0x07));
    CHECK(ReadCpu(cartridge, 0x8000) == 7);
}

TEST_CASE("Mapper 7 ignores non-bank bits and wraps to available PRG")
{
    auto cartridge = MakeAxromCartridge(4); // 2 x 32 KiB banks

    REQUIRE(cartridge.CpuWrite(0x9000, 0xFF));
    CHECK(ReadCpu(cartridge, 0x8000) == 1);

    REQUIRE(cartridge.CpuWrite(0x9000, 0x10));
    CHECK(ReadCpu(cartridge, 0x8000) == 0);
}

TEST_CASE("Mapper 7 bit 4 selects one-screen nametable mirroring")
{
    auto cartridge = MakeAxromCartridge(4);

    CHECK(cartridge.CurrentMirroring() == dendyforge::Mirroring::OneScreenLower);

    REQUIRE(cartridge.CpuWrite(0x8000, 0x10));
    CHECK(cartridge.CurrentMirroring() == dendyforge::Mirroring::OneScreenUpper);

    REQUIRE(cartridge.CpuWrite(0x8000, 0x00));
    CHECK(cartridge.CurrentMirroring() == dendyforge::Mirroring::OneScreenLower);
}

TEST_CASE("Mapper 7 provides fixed writable 8 KiB CHR RAM")
{
    auto cartridge = MakeAxromCartridge(4);

    REQUIRE(cartridge.PpuWrite(0x0000, 0x12));
    REQUIRE(cartridge.PpuWrite(0x1FFF, 0x34));
    CHECK(ReadPpu(cartridge, 0x0000) == 0x12);
    CHECK(ReadPpu(cartridge, 0x1FFF) == 0x34);

    REQUIRE(cartridge.CpuWrite(0x8000, 0x11));
    CHECK(ReadPpu(cartridge, 0x0000) == 0x12);
    CHECK(ReadPpu(cartridge, 0x1FFF) == 0x34);
}
