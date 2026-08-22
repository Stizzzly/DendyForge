#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "cartridge/cartridge.hpp"
#include "ppu/ppu.hpp"

namespace
{

// Builds an MMC1 cartridge whose 16 KiB PRG banks are each filled with
// their bank number and whose 4 KiB CHR banks are each filled with their
// bank number, so a single read identifies the mapped bank.
dendyforge::Cartridge MakeMmc1Cartridge(
    std::uint8_t prgBanks,
    std::uint8_t chrBanks,
    std::uint8_t flags6Low = 0x00)
{
    dendyforge::INesHeader header{};
    header.prgRomBanks = prgBanks;
    header.chrRomBanks = chrBanks;
    header.flags6 = static_cast<std::uint8_t>(0x10 | (flags6Low & 0x0F));

    std::vector<std::uint8_t> prgRom(
        static_cast<std::size_t>(prgBanks) * 16 * 1024);
    for (std::uint8_t bank = 0; bank < prgBanks; ++bank)
    {
        std::fill_n(prgRom.begin() + bank * 16 * 1024, 16 * 1024, bank);
    }

    std::vector<std::uint8_t> chrRom(
        static_cast<std::size_t>(chrBanks) * 8 * 1024);
    for (std::size_t bank4 = 0;
         bank4 < chrRom.size() / (4 * 1024);
         ++bank4)
    {
        std::fill_n(chrRom.begin() + bank4 * 4 * 1024,
                    4 * 1024,
                    static_cast<std::uint8_t>(bank4));
    }

    return dendyforge::Cartridge(
        header, std::move(prgRom), std::move(chrRom));
}

void WriteRegister(
    dendyforge::Cartridge& cartridge,
    std::uint16_t address,
    std::uint8_t value)
{
    for (std::uint8_t bit = 0; bit < 5; ++bit)
    {
        REQUIRE(cartridge.CpuWrite(
            address, static_cast<std::uint8_t>((value >> bit) & 0x01)));
    }
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

TEST_CASE("Mapper 1 powers on with the last bank mapped at $C000")
{
    auto cartridge = MakeMmc1Cartridge(4, 2);

    CHECK(ReadCpu(cartridge, 0x8000) == 0);
    CHECK(ReadCpu(cartridge, 0xBFFF) == 0);
    CHECK(ReadCpu(cartridge, 0xC000) == 3);
    CHECK(ReadCpu(cartridge, 0xFFFF) == 3);
}

TEST_CASE("Mapper 1 loads a register from five serial writes, LSB first")
{
    auto cartridge = MakeMmc1Cartridge(4, 2);

    WriteRegister(cartridge, 0xE000, 0x02);

    CHECK(ReadCpu(cartridge, 0x8000) == 2);
    CHECK(ReadCpu(cartridge, 0xC000) == 3);
}

TEST_CASE("Mapper 1 selects the register from the fifth write's address")
{
    auto cartridge = MakeMmc1Cartridge(4, 2);

    WriteRegister(cartridge, 0xA000, 0x02);
    CHECK(ReadPpu(cartridge, 0x0000) == 2);
    CHECK(ReadPpu(cartridge, 0x1000) == 3);

    // Four data bits sent toward $E000 must not load the PRG register;
    // the fifth write, aimed at $A000, loads CHR bank 0 instead.
    cartridge.CpuWrite(0xE000, 0x00);
    cartridge.CpuWrite(0xE000, 0x00);
    cartridge.CpuWrite(0xE000, 0x00);
    cartridge.CpuWrite(0xE000, 0x00);
    CHECK(ReadCpu(cartridge, 0x8000) == 0);

    cartridge.CpuWrite(0xA000, 0x00);
    CHECK(ReadPpu(cartridge, 0x0000) == 0);
    CHECK(ReadPpu(cartridge, 0x1000) == 1);
    CHECK(ReadCpu(cartridge, 0x8000) == 0);
}

TEST_CASE("Mapper 1 32 KiB PRG modes ignore the bank low bit")
{
    auto cartridge = MakeMmc1Cartridge(4, 2);

    SUBCASE("control mode 0")
    {
        WriteRegister(cartridge, 0x8000, 0x00);
    }
    SUBCASE("control mode 1")
    {
        WriteRegister(cartridge, 0x8000, 0x04);
    }

    WriteRegister(cartridge, 0xE000, 0x01);
    CHECK(ReadCpu(cartridge, 0x8000) == 0);
    CHECK(ReadCpu(cartridge, 0xC000) == 1);

    WriteRegister(cartridge, 0xE000, 0x03);
    CHECK(ReadCpu(cartridge, 0x8000) == 2);
    CHECK(ReadCpu(cartridge, 0xC000) == 3);
}

TEST_CASE("Mapper 1 PRG mode 2 fixes the first bank at $8000")
{
    SUBCASE("four PRG banks")
    {
        auto cartridge = MakeMmc1Cartridge(4, 2);
        WriteRegister(cartridge, 0x8000, 0x08);

        WriteRegister(cartridge, 0xE000, 0x03);
        CHECK(ReadCpu(cartridge, 0x8000) == 0);
        CHECK(ReadCpu(cartridge, 0xC000) == 3);

        WriteRegister(cartridge, 0xE000, 0x02);
        CHECK(ReadCpu(cartridge, 0x8000) == 0);
        CHECK(ReadCpu(cartridge, 0xC000) == 2);
    }

    SUBCASE("bank numbers wrap to the available banks")
    {
        auto cartridge = MakeMmc1Cartridge(8, 2);
        WriteRegister(cartridge, 0x8000, 0x08);

        WriteRegister(cartridge, 0xE000, 0x0F);
        CHECK(ReadCpu(cartridge, 0x8000) == 0);
        CHECK(ReadCpu(cartridge, 0xC000) == 7);
    }
}

TEST_CASE("Mapper 1 PRG mode 3 fixes the last bank at $C000")
{
    auto cartridge = MakeMmc1Cartridge(4, 2);
    WriteRegister(cartridge, 0x8000, 0x0C);

    WriteRegister(cartridge, 0xE000, 0x02);
    CHECK(ReadCpu(cartridge, 0x8000) == 2);
    CHECK(ReadCpu(cartridge, 0xC000) == 3);
}

TEST_CASE("Mapper 1 4 KiB CHR mode maps two independent pattern banks")
{
    auto cartridge = MakeMmc1Cartridge(4, 2);
    WriteRegister(cartridge, 0x8000, 0x10);

    WriteRegister(cartridge, 0xA000, 0x01);
    WriteRegister(cartridge, 0xC000, 0x02);
    CHECK(ReadPpu(cartridge, 0x0000) == 1);
    CHECK(ReadPpu(cartridge, 0x0FFF) == 1);
    CHECK(ReadPpu(cartridge, 0x1000) == 2);
    CHECK(ReadPpu(cartridge, 0x1FFF) == 2);

    WriteRegister(cartridge, 0xA000, 0x03);
    CHECK(ReadPpu(cartridge, 0x0000) == 3);
    CHECK(ReadPpu(cartridge, 0x1000) == 2);
}

TEST_CASE("Mapper 1 8 KiB CHR mode ignores the low bit and CHR bank 1")
{
    auto cartridge = MakeMmc1Cartridge(4, 2);
    WriteRegister(cartridge, 0x8000, 0x00);

    WriteRegister(cartridge, 0xA000, 0x01);
    CHECK(ReadPpu(cartridge, 0x0000) == 0);
    CHECK(ReadPpu(cartridge, 0x1000) == 1);

    WriteRegister(cartridge, 0xA000, 0x02);
    CHECK(ReadPpu(cartridge, 0x0000) == 2);
    CHECK(ReadPpu(cartridge, 0x1000) == 3);

    WriteRegister(cartridge, 0xC000, 0x00);
    CHECK(ReadPpu(cartridge, 0x0000) == 2);
    CHECK(ReadPpu(cartridge, 0x1000) == 3);
}

TEST_CASE("Mapper 1 CHR RAM boards pass PPU writes through")
{
    auto cartridge = MakeMmc1Cartridge(4, 0);

    REQUIRE(cartridge.PpuWrite(0x0000, 0xA5));
    REQUIRE(cartridge.PpuWrite(0x1FFF, 0x5A));
    CHECK(ReadPpu(cartridge, 0x0000) == 0xA5);
    CHECK(ReadPpu(cartridge, 0x1FFF) == 0x5A);

    // 4 KiB CHR mode must not bank CHR RAM either.
    WriteRegister(cartridge, 0x8000, 0x10);
    REQUIRE(cartridge.PpuWrite(0x1000, 0x33));
    CHECK(ReadPpu(cartridge, 0x1000) == 0x33);
    CHECK(ReadPpu(cartridge, 0x0000) == 0xA5);
}

TEST_CASE("Mapper 1 register writes never modify PRG ROM")
{
    auto cartridge = MakeMmc1Cartridge(4, 2);
    const auto original = cartridge.PRGRom();

    WriteRegister(cartridge, 0x8000, 0x1F);
    WriteRegister(cartridge, 0xA000, 0x1F);
    WriteRegister(cartridge, 0xC000, 0x1F);
    WriteRegister(cartridge, 0xE000, 0x1F);
    cartridge.CpuWrite(0x8000, 0x80);
    cartridge.CpuWrite(0xE000, 0x01);

    CHECK(cartridge.PRGRom() == original);
}

TEST_CASE("Mapper 1 reset write restores PRG mode 3 and drops partial sequences")
{
    auto cartridge = MakeMmc1Cartridge(4, 2);

    WriteRegister(cartridge, 0x8000, 0x00);
    WriteRegister(cartridge, 0xE000, 0x03);
    CHECK(ReadCpu(cartridge, 0x8000) == 2);
    CHECK(ReadCpu(cartridge, 0xC000) == 3);

    // Three stray serial writes must not reach the PRG register.
    cartridge.CpuWrite(0xE000, 0x00);
    cartridge.CpuWrite(0xE000, 0x00);
    cartridge.CpuWrite(0xE000, 0x00);
    CHECK(ReadCpu(cartridge, 0x8000) == 2);

    // A write with bit 7 set resets the sequence and forces PRG mode 3.
    REQUIRE(cartridge.CpuWrite(0xE000, 0x80));
    CHECK(ReadCpu(cartridge, 0x8000) == 3);
    CHECK(ReadCpu(cartridge, 0xC000) == 3);

    // The next single bit starts a fresh sequence, so it changes nothing.
    cartridge.CpuWrite(0xE000, 0x00);
    CHECK(ReadCpu(cartridge, 0x8000) == 3);

    WriteRegister(cartridge, 0xE000, 0x00);
    CHECK(ReadCpu(cartridge, 0x8000) == 0);
    CHECK(ReadCpu(cartridge, 0xC000) == 3);
}

TEST_CASE("Mapper 1 drives all four nametable arrangements")
{
    auto cartridge = MakeMmc1Cartridge(4, 2, dendyforge::FLAG_MIRRORING);
    CHECK(cartridge.CurrentMirroring() == dendyforge::Mirroring::Vertical);

    WriteRegister(cartridge, 0x8000, 0x00);
    CHECK(cartridge.CurrentMirroring() ==
          dendyforge::Mirroring::OneScreenLower);
    WriteRegister(cartridge, 0x8000, 0x01);
    CHECK(cartridge.CurrentMirroring() ==
          dendyforge::Mirroring::OneScreenUpper);
    WriteRegister(cartridge, 0x8000, 0x02);
    CHECK(cartridge.CurrentMirroring() == dendyforge::Mirroring::Vertical);
    WriteRegister(cartridge, 0x8000, 0x03);
    CHECK(cartridge.CurrentMirroring() == dendyforge::Mirroring::Horizontal);
}

namespace
{

void PointPpuAddress(dendyforge::PPU& ppu, std::uint16_t address)
{
    ppu.CpuWrite(0x2006, static_cast<std::uint8_t>(address >> 8));
    ppu.CpuWrite(0x2006, static_cast<std::uint8_t>(address & 0xFF));
}

std::uint8_t ReadPpuVram(dendyforge::PPU& ppu, std::uint16_t address)
{
    PointPpuAddress(ppu, address);
    ppu.CpuRead(0x2007);
    return ppu.CpuRead(0x2007);
}

} // namespace

TEST_CASE("PPU nametable access follows the MMC1 arrangement live")
{
    auto cartridge = MakeMmc1Cartridge(4, 2);

    const std::uint16_t windows[4] = {0x2000, 0x2400, 0x2800, 0x2C00};

    // Expected value in each window after writing one marker through
    // $2000. Both one-screen arrangements alias every window to the
    // single visible table, so both show the marker everywhere; the
    // lower/upper distinction is covered by CurrentMirroring() above.
    const std::uint8_t expected[4][4] = {
        {1, 1, 1, 1}, // control 0: one-screen lower.
        {1, 1, 1, 1}, // control 1: one-screen upper.
        {1, 0, 1, 0}, // control 2: vertical arrangement.
        {1, 1, 0, 0}, // control 3: horizontal arrangement.
    };

    for (std::uint8_t control = 0; control < 4; ++control)
    {
        dendyforge::PPU ppu;
        ppu.ConnectCartridge(&cartridge);

        WriteRegister(cartridge, 0x8000, control);

        PointPpuAddress(ppu, 0x2000);
        ppu.CpuWrite(0x2007, 0x42);

        for (int window = 0; window < 4; ++window)
        {
            const std::uint8_t value = ReadPpuVram(ppu, windows[window]);

            INFO("control " << static_cast<int>(control)
                            << " window " << std::hex << windows[window]);
            CHECK(value == (expected[control][window] ? 0x42 : 0x00));
        }
    }
}
