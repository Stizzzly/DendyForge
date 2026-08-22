#include <doctest/doctest.h>

#include <cstdint>

#include "bus/bus.hpp"
#include "ppu/ppu.hpp"
#include "zapper/zapper.hpp"

namespace
{

// Renders an opaque (color 3) background across the whole screen with
// the given palette entry at $3F03, producing a uniformly bright or dark
// framebuffer.
void RenderScreen(dendyforge::PPU& ppu, std::uint8_t paletteIndex)
{
    ppu.CpuWrite(0x2001, 0x0A);
    for (std::uint16_t row = 0; row < 8; ++row)
    {
        ppu.PpuWrite(row, 0xFF);
        ppu.PpuWrite(row + 8, 0xFF);
    }
    ppu.PpuWrite(0x2000, 0x00);
    ppu.PpuWrite(0x3F00, 0x0F);
    ppu.PpuWrite(0x3F03, paletteIndex);
    ppu.RenderBackground();
}

} // namespace

TEST_CASE("The Zapper reports light only on the aimed scanline past the aim column")
{
    dendyforge::PPU ppu;
    RenderScreen(ppu, 0x30); // white

    dendyforge::Zapper zapper;
    zapper.SetAim(10, 5);

    // Beam past the aim column on the aim scanline: light detected.
    CHECK((zapper.ReadPort(5, 21, ppu.FrameBuffer()) & 0x08) == 0);

    // Wrong scanline: dark.
    CHECK((zapper.ReadPort(4, 21, ppu.FrameBuffer()) & 0x08) != 0);
    CHECK((zapper.ReadPort(6, 21, ppu.FrameBuffer()) & 0x08) != 0);

    // Beam has not reached the aim column yet: dark.
    CHECK((zapper.ReadPort(5, 10, ppu.FrameBuffer()) & 0x08) != 0);
    CHECK((zapper.ReadPort(5, 5, ppu.FrameBuffer()) & 0x08) != 0);
}

TEST_CASE("The Zapper ignores dark pixels and off-screen aims")
{
    dendyforge::PPU ppu;
    RenderScreen(ppu, 0x0F); // black

    dendyforge::Zapper zapper;
    zapper.SetAim(10, 5);
    CHECK((zapper.ReadPort(5, 21, ppu.FrameBuffer()) & 0x08) != 0);

    zapper.SetAim(300, 5);
    CHECK((zapper.ReadPort(5, 21, ppu.FrameBuffer()) & 0x08) != 0);

    zapper.SetAim(10, 300);
    CHECK((zapper.ReadPort(5, 21, ppu.FrameBuffer()) & 0x08) != 0);
}

TEST_CASE("The Zapper reports the trigger in bit 4")
{
    dendyforge::PPU ppu;
    RenderScreen(ppu, 0x30);

    dendyforge::Zapper zapper;
    zapper.SetAim(10, 5);
    CHECK((zapper.ReadPort(5, 21, ppu.FrameBuffer()) & 0x10) == 0);

    zapper.SetTrigger(true);
    CHECK((zapper.ReadPort(5, 21, ppu.FrameBuffer()) & 0x10) != 0);

    zapper.SetTrigger(false);
    CHECK((zapper.ReadPort(5, 21, ppu.FrameBuffer()) & 0x10) == 0);
}

TEST_CASE("The bus exposes the Zapper on $4017 reads")
{
    dendyforge::Bus bus;

    // The idle bus PPU sits on the pre-render line, which no aim can
    // match, so the sensor reads dark.
    bus.SecondaryZapper().SetAim(10, 5);
    CHECK((bus.CpuRead(0x4017) & 0x08) != 0);

    bus.SecondaryZapper().SetTrigger(true);
    CHECK((bus.CpuRead(0x4017) & 0x10) != 0);
    CHECK((bus.CpuRead(0x4017) & 0x08) != 0);
}
