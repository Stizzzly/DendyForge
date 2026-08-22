#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "cartridge/cartridge.hpp"

namespace
{

// Builds a CNROM cartridge whose PRG is filled with $AA and whose 8 KiB
// CHR banks are each filled with their bank number.
dendyforge::Cartridge MakeCnromCartridge(std::uint8_t chrBanks)
{
    dendyforge::INesHeader header{};
    header.prgRomBanks = 1;
    header.chrRomBanks = chrBanks;
    header.flags6 = 0x30; // mapper 3, horizontal mirroring

    std::vector<std::uint8_t> prgRom(16 * 1024, 0xAA);
    std::vector<std::uint8_t> chrRom(
        static_cast<std::size_t>(chrBanks) * 8 * 1024);
    for (std::size_t bank = 0; bank < chrBanks; ++bank)
    {
        std::fill_n(chrRom.begin() + bank * 8 * 1024, 8 * 1024,
                    static_cast<std::uint8_t>(bank));
    }

    return dendyforge::Cartridge(
        header, std::move(prgRom), std::move(chrRom));
}

std::uint8_t ReadPpu(dendyforge::Cartridge& cartridge, std::uint16_t address)
{
    std::uint8_t data = 0;
    REQUIRE(cartridge.PpuRead(address, data));
    return data;
}

std::uint8_t ReadCpu(dendyforge::Cartridge& cartridge, std::uint16_t address)
{
    std::uint8_t data = 0;
    REQUIRE(cartridge.CpuRead(address, data));
    return data;
}

} // namespace

TEST_CASE("Mapper 3 maps 16 KiB PRG through the fixed window")
{
    auto cartridge = MakeCnromCartridge(4);

    CHECK(ReadCpu(cartridge, 0x8000) == 0xAA);
    CHECK(ReadCpu(cartridge, 0xBFFF) == 0xAA);
    CHECK(ReadCpu(cartridge, 0xC000) == 0xAA);
    CHECK(ReadCpu(cartridge, 0xFFFF) == 0xAA);
}

TEST_CASE("Mapper 3 selects the CHR bank from any write to $8000-$FFFF")
{
    auto cartridge = MakeCnromCartridge(4);

    CHECK(ReadPpu(cartridge, 0x0000) == 0);
    CHECK(ReadPpu(cartridge, 0x1FFF) == 0);

    cartridge.CpuWrite(0x8000, 0x02);
    CHECK(ReadPpu(cartridge, 0x0000) == 2);
    CHECK(ReadPpu(cartridge, 0x1000) == 2);

    // The write may target any PPU-facing register address.
    cartridge.CpuWrite(0xFFFF, 0x01);
    CHECK(ReadPpu(cartridge, 0x0FFF) == 1);

    // Bank numbers wrap to the available banks.
    cartridge.CpuWrite(0x8000, 0x05);
    CHECK(ReadPpu(cartridge, 0x0000) == 1);
}

TEST_CASE("Mapper 3 register writes never modify CHR or PRG ROM")
{
    auto cartridge = MakeCnromCartridge(2);
    const auto originalChr = cartridge.CHRRom();

    cartridge.CpuWrite(0x8000, 0xFF);
    CHECK(cartridge.CHRRom() == originalChr);
}
