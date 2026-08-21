#include <doctest/doctest.h>

#include "bus/bus.hpp"

namespace
{

void SetPpuAddress(dendyforge::Bus& bus, std::uint16_t address)
{
    bus.CpuWrite(0x2006, address >> 8);
    bus.CpuWrite(0x2006, address & 0xFF);
}

}

TEST_CASE("PPU register range is mirrored through the CPU bus")
{
    dendyforge::Bus bus;

    SetPpuAddress(bus, 0x2000);
    bus.CpuWrite(0x2007, 0x42);

    CHECK(bus.VideoProcessor().PpuRead(0x2000) == 0x42);
}

TEST_CASE("PPU data port buffers nametable reads and increments its address")
{
    dendyforge::Bus bus;
    bus.VideoProcessor().PpuWrite(0x2000, 0x12);
    bus.VideoProcessor().PpuWrite(0x2001, 0x34);

    SetPpuAddress(bus, 0x2000);
    CHECK(bus.CpuRead(0x2007) == 0x00);
    CHECK(bus.CpuRead(0x2007) == 0x12);

    SetPpuAddress(bus, 0x2000);
    bus.CpuWrite(0x2008, 0x04);
    bus.CpuWrite(0x2007, 0x56);
    bus.CpuWrite(0x2007, 0x78);
    CHECK(bus.VideoProcessor().PpuRead(0x2000) == 0x56);
    CHECK(bus.VideoProcessor().PpuRead(0x2020) == 0x78);
}

TEST_CASE("PPU mirrors nametable and palette memory")
{
    dendyforge::PPU ppu;

    ppu.PpuWrite(0x2000, 0xAB);
    CHECK(ppu.PpuRead(0x2400) == 0xAB);
    CHECK(ppu.PpuRead(0x3000) == 0xAB);

    ppu.PpuWrite(0x3F10, 0x0C);
    CHECK(ppu.PpuRead(0x3F00) == 0x0C);
    CHECK(ppu.PpuRead(0x3F30) == 0x0C);
}

TEST_CASE("PPU enters VBlank and raises one NMI when enabled")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2000, 0x80);

    for (int cycle = 0; cycle < 242 * 341 + 2; ++cycle)
    {
        ppu.Clock();
    }

    CHECK(ppu.PollNmi());
    CHECK_FALSE(ppu.PollNmi());
    CHECK((ppu.CpuRead(0x2002) & 0x80) != 0);
}
