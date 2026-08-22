#include <doctest/doctest.h>

#include <algorithm>

#include "ines/ines_reader.hpp"
#include "cartridge/cartridge.hpp"
#include "cpu/cpu_test_support.hpp"

TEST_CASE("iNES reader loads the CPU test cartridge")
{
    dendyforge::INesReader reader;
    const auto path = dendyforge::test::RomPath("cpu_test.nes");

    REQUIRE_MESSAGE(reader.Load(path.string()), "Unable to load ROM: " << path.string());

    dendyforge::Cartridge cartridge(
        reader.Header(),
        reader.TakePRGRom(),
        reader.TakeCHRRom());

    CHECK(cartridge.Info().Mapper() == 0);
    CHECK(cartridge.PRGRom().size() == 32 * 1024);
    CHECK(cartridge.CHRRom().size() == 8 * 1024);
}

TEST_CASE("Mapper 2 switches the lower PRG bank and provides CHR RAM")
{
    dendyforge::INesHeader header{};
    header.prgRomBanks = 4;
    header.chrRomBanks = 0;
    header.flags6 = 0x20;

    std::vector<std::uint8_t> prgRom(4 * 16 * 1024);
    for (std::uint8_t bank = 0; bank < 4; ++bank)
    {
        std::fill_n(prgRom.begin() + bank * 16 * 1024,
                    16 * 1024, bank);
    }

    dendyforge::Cartridge cartridge(header, std::move(prgRom), {});
    std::uint8_t data = 0;

    REQUIRE(cartridge.CpuRead(0x8000, data));
    CHECK(data == 0);
    REQUIRE(cartridge.CpuRead(0xC000, data));
    CHECK(data == 3);

    REQUIRE(cartridge.CpuWrite(0x8000, 0x02));
    REQUIRE(cartridge.CpuRead(0x8000, data));
    CHECK(data == 2);
    REQUIRE(cartridge.CpuRead(0xC000, data));
    CHECK(data == 3);

    REQUIRE(cartridge.CpuWrite(0xFFFF, 0x05));
    REQUIRE(cartridge.CpuRead(0x8000, data));
    CHECK(data == 1);

    REQUIRE(cartridge.PpuWrite(0x1FFF, 0xA5));
    REQUIRE(cartridge.PpuRead(0x1FFF, data));
    CHECK(data == 0xA5);
}

TEST_CASE("Cartridge provides 8 KiB PRG RAM when iNES declares none")
{
    dendyforge::INesHeader header{};
    header.prgRomBanks = 1;
    std::vector<std::uint8_t> prgRom(16 * 1024);
    dendyforge::Cartridge cartridge(header, std::move(prgRom), {});

    REQUIRE(cartridge.CpuWrite(0x6000, 0xA5));
    REQUIRE(cartridge.CpuWrite(0x7FFF, 0x5A));

    std::uint8_t data = 0;
    REQUIRE(cartridge.CpuRead(0x6000, data));
    CHECK(data == 0xA5);
    REQUIRE(cartridge.CpuRead(0x7FFF, data));
    CHECK(data == 0x5A);
}
