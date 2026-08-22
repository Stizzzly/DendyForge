#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "cartridge/cartridge.hpp"
#include "ppu/ppu.hpp"

namespace
{

// Builds an MMC3 cartridge whose 8 KiB PRG banks are filled with their
// bank number and whose 1 KiB CHR banks are filled with their bank
// number, so single reads identify the mapped bank.
dendyforge::Cartridge MakeMmc3Cartridge(
    std::uint8_t prgBanks, std::uint8_t chrBanks,
    std::uint8_t flags6Low = 0x00)
{
    dendyforge::INesHeader header{};
    header.prgRomBanks = prgBanks;
    header.chrRomBanks = chrBanks;
    header.flags6 = static_cast<std::uint8_t>(0x40 | (flags6Low & 0x0F));

    std::vector<std::uint8_t> prgRom(
        static_cast<std::size_t>(prgBanks) * 16 * 1024);
    for (std::size_t bank8k = 0;
         bank8k < prgRom.size() / (8 * 1024);
         ++bank8k)
    {
        std::fill_n(prgRom.begin() + bank8k * 8 * 1024, 8 * 1024,
                    static_cast<std::uint8_t>(bank8k));
    }

    std::vector<std::uint8_t> chrRom(
        static_cast<std::size_t>(chrBanks) * 8 * 1024);
    for (std::size_t bank1k = 0;
         bank1k < chrRom.size() / 1024;
         ++bank1k)
    {
        std::fill_n(chrRom.begin() + bank1k * 1024, 1024,
                    static_cast<std::uint8_t>(bank1k));
    }

    return dendyforge::Cartridge(
        header, std::move(prgRom), std::move(chrRom));
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

TEST_CASE("Mapper 4 PRG mode 0 fixes the last banks and banks R6/R7 low")
{
    auto cartridge = MakeMmc3Cartridge(4, 4); // 8 x 8 KiB PRG banks

    cartridge.CpuWrite(0x8000, 0x06); // select R6
    cartridge.CpuWrite(0x8001, 0x02);
    cartridge.CpuWrite(0x8000, 0x07); // select R7
    cartridge.CpuWrite(0x8001, 0x03);

    CHECK(ReadCpu(cartridge, 0x8000) == 2);
    CHECK(ReadCpu(cartridge, 0x9FFF) == 2);
    CHECK(ReadCpu(cartridge, 0xA000) == 3);
    CHECK(ReadCpu(cartridge, 0xBFFF) == 3);
    CHECK(ReadCpu(cartridge, 0xC000) == 6); // penultimate
    CHECK(ReadCpu(cartridge, 0xE000) == 7); // last
}

TEST_CASE("Mapper 4 PRG mode 1 swaps R6 into $C000")
{
    auto cartridge = MakeMmc3Cartridge(4, 4);

    cartridge.CpuWrite(0x8000, 0x46); // PRG mode 1, select R6
    cartridge.CpuWrite(0x8001, 0x05);

    CHECK(ReadCpu(cartridge, 0x8000) == 6);
    CHECK(ReadCpu(cartridge, 0xC000) == 5);
    CHECK(ReadCpu(cartridge, 0xE000) == 7);
}

TEST_CASE("Mapper 4 PRG bank numbers wrap to the available banks")
{
    auto cartridge = MakeMmc3Cartridge(2, 4); // 4 x 8 KiB banks

    cartridge.CpuWrite(0x8000, 0x06);
    cartridge.CpuWrite(0x8001, 0x0F);
    CHECK(ReadCpu(cartridge, 0x8000) == 3);

    CHECK(ReadCpu(cartridge, 0xC000) == 2);
    CHECK(ReadCpu(cartridge, 0xE000) == 3);
}

TEST_CASE("Mapper 4 CHR mode 0 banks 2 KiB low and 1 KiB high")
{
    auto cartridge = MakeMmc3Cartridge(4, 4); // 8 x 1 KiB CHR banks

    // R0 selects 1K banks 2-3, R1 selects 1K banks 0-1, R2..R5 select
    // 1K banks 6,0,1,2 directly.
    cartridge.CpuWrite(0x8000, 0x00);
    cartridge.CpuWrite(0x8001, 0x02);
    {
        std::uint8_t early = 0;
        cartridge.PpuRead(0x0000, early);
        const auto& chr = cartridge.CHRRom();
        std::printf("read=%d raw: [400]=%d [800]=%d [C00]=%d [1000]=%d [1800]=%d size=%d\n",
                    (int)early, (int)chr[0x400], (int)chr[0x800],
                    (int)chr[0xC00], (int)chr[0x1000], (int)chr[0x1800],
                    (int)chr.size());
    }
    cartridge.CpuWrite(0x8000, 0x01);
    cartridge.CpuWrite(0x8001, 0x00);
    cartridge.CpuWrite(0x8000, 0x02);
    cartridge.CpuWrite(0x8001, 0x06);
    cartridge.CpuWrite(0x8000, 0x03);
    cartridge.CpuWrite(0x8001, 0x00);
    cartridge.CpuWrite(0x8000, 0x04);
    cartridge.CpuWrite(0x8001, 0x01);
    cartridge.CpuWrite(0x8000, 0x05);
    cartridge.CpuWrite(0x8001, 0x02);
    cartridge.CpuWrite(0x8000, 0x00); // CHR/PRG mode 0

    // The 2 KiB register's low bit is masked off and the half within the
    // pair comes from PPU A10.
    std::uint8_t probeData = 0;
    cartridge.PpuRead(0x0000, probeData);
    std::printf("R0 read: %d chrRomSize=%d\n", (int)probeData,
                (int)cartridge.CHRRom().size());
    CHECK(ReadPpu(cartridge, 0x0000) == 2);
    CHECK(ReadPpu(cartridge, 0x03FF) == 2);
    CHECK(ReadPpu(cartridge, 0x0400) == 3);
    CHECK(ReadPpu(cartridge, 0x07FF) == 3);
    CHECK(ReadPpu(cartridge, 0x0800) == 0);
    CHECK(ReadPpu(cartridge, 0x0FFF) == 1);
    CHECK(ReadPpu(cartridge, 0x1000) == 6);
    CHECK(ReadPpu(cartridge, 0x13FF) == 6);
    CHECK(ReadPpu(cartridge, 0x1400) == 0);
    CHECK(ReadPpu(cartridge, 0x1800) == 1);
    CHECK(ReadPpu(cartridge, 0x1C00) == 2);
}

TEST_CASE("Mapper 4 CHR mode 1 banks 1 KiB low and 2 KiB high")
{
    auto cartridge = MakeMmc3Cartridge(4, 4);

    cartridge.CpuWrite(0x8000, 0x00);
    cartridge.CpuWrite(0x8001, 0x02); // R0 selects 1K banks 2-3
    cartridge.CpuWrite(0x8000, 0x02);
    cartridge.CpuWrite(0x8001, 0x05); // R2 = 1K bank 5
    cartridge.CpuWrite(0x8000, 0x80); // CHR mode 1

    CHECK(ReadPpu(cartridge, 0x0000) == 5);
    CHECK(ReadPpu(cartridge, 0x03FF) == 5);
    CHECK(ReadPpu(cartridge, 0x0400) == 0);
    CHECK(ReadPpu(cartridge, 0x1000) == 2);
    CHECK(ReadPpu(cartridge, 0x17FF) == 3);
    CHECK(ReadPpu(cartridge, 0x1400) == 3);
    CHECK(ReadPpu(cartridge, 0x1800) == 0);
}

TEST_CASE("Mapper 4 switches the nametable arrangement through $A000")
{
    auto cartridge = MakeMmc3Cartridge(4, 4, dendyforge::FLAG_MIRRORING);
    CHECK(cartridge.CurrentMirroring() == dendyforge::Mirroring::Vertical);

    cartridge.CpuWrite(0xA000, 0x00);
    CHECK(cartridge.CurrentMirroring() ==
          dendyforge::Mirroring::Horizontal);
    cartridge.CpuWrite(0xA000, 0x01);
    CHECK(cartridge.CurrentMirroring() ==
          dendyforge::Mirroring::Vertical);
}

TEST_CASE("Mapper 4 asserts the scanline IRQ one clock after the latch counts down")
{
    auto cartridge = MakeMmc3Cartridge(4, 4);

    cartridge.CpuWrite(0xC000, 2); // latch = 2
    cartridge.CpuWrite(0xE001, 0x00); // enable
    cartridge.CpuWrite(0xC001, 0x00); // reload request

    REQUIRE_FALSE(cartridge.IrqPending());
    cartridge.PpuScanlineClock(); // reload (2)
    cartridge.PpuScanlineClock(); // 1
    REQUIRE_FALSE(cartridge.IrqPending());
    cartridge.PpuScanlineClock(); // 0
    REQUIRE_FALSE(cartridge.IrqPending());
    cartridge.PpuScanlineClock(); // counter at zero: fire and reload
    CHECK(cartridge.IrqPending());
}

TEST_CASE("Mapper 4 IRQ disable acknowledges and enable waits for the next wrap")
{
    auto cartridge = MakeMmc3Cartridge(4, 4);

    cartridge.CpuWrite(0xC000, 0); // latch = 0: every clock fires
    cartridge.CpuWrite(0xE001, 0x00);

    cartridge.PpuScanlineClock();
    CHECK(cartridge.IrqPending());

    cartridge.CpuWrite(0xE000, 0x00); // disable + acknowledge
    CHECK_FALSE(cartridge.IrqPending());

    cartridge.CpuWrite(0xE001, 0x00); // re-enable
    CHECK_FALSE(cartridge.IrqPending());

    cartridge.PpuScanlineClock(); // fires again when enabled
    CHECK(cartridge.IrqPending());
}

TEST_CASE("Mapper 4 does not assert the IRQ while disabled at the wrap")
{
    auto cartridge = MakeMmc3Cartridge(4, 4);

    cartridge.CpuWrite(0xC000, 0);
    cartridge.PpuScanlineClock();
    cartridge.PpuScanlineClock();
    CHECK_FALSE(cartridge.IrqPending());
}

TEST_CASE("Mapper 4 register writes never modify PRG ROM")
{
    auto cartridge = MakeMmc3Cartridge(4, 4);
    const auto original = cartridge.PRGRom();

    cartridge.CpuWrite(0x8000, 0x00);
    cartridge.CpuWrite(0x8001, 0xFF);
    cartridge.CpuWrite(0xA000, 0x01);
    cartridge.CpuWrite(0xC000, 0xFF);
    cartridge.CpuWrite(0xE001, 0xFF);

    CHECK(cartridge.PRGRom() == original);
}
