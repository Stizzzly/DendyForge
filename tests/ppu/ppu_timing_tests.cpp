#include <doctest/doctest.h>

#include <cstdint>

#include "ppu/ppu.hpp"

namespace
{

// Power-on state is scanline -1, cycle 0. Frames are 262 scanlines of 341
// dots; the odd-frame skip never applies here because these tests leave
// rendering disabled, so absolute dot counts are deterministic. A frame is
// 89342 dots and the VBlank flag sets while processing the dot at scanline
// 241, cycle 1.
constexpr std::int64_t DotsPerFrame = 262 * 341;
constexpr std::int64_t DotsUntilVBlankSetDot = 242 * 341 + 1;

// Clocks until the dot at (scanline, cycle) is the next one to execute.
std::int64_t DotsUntil(std::int16_t scanline, std::int16_t cycle)
{
    return (static_cast<std::int64_t>(scanline) + 1) * 341 + cycle;
}

void ClockDots(dendyforge::PPU& ppu, std::int64_t dots)
{
    for (std::int64_t dot = 0; dot < dots; ++dot)
    {
        ppu.Clock();
    }
}

} // namespace

TEST_CASE("VBlank sets at scanline 241 cycle 1 and raises the NMI line")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2000, 0x80);

    ClockDots(ppu, DotsUntilVBlankSetDot + 1);

    CHECK(ppu.NmiLineLevel());
    CHECK((ppu.CpuRead(0x2002) & 0x80) != 0);
    CHECK_FALSE(ppu.NmiLineLevel());
    CHECK(ppu.ConsumeFrameComplete());
    CHECK_FALSE(ppu.ConsumeFrameComplete());
}

TEST_CASE("A $2002 read one dot before the set dot suppresses flag and NMI")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2000, 0x80);

    // Stop with the set dot pending: the read lands one dot before it.
    ClockDots(ppu, DotsUntilVBlankSetDot);
    CHECK((ppu.CpuRead(0x2002) & 0x80) == 0);

    // The rest of this frame's VBlank must stay suppressed.
    ClockDots(ppu, DotsUntil(241, 100) - DotsUntilVBlankSetDot);
    CHECK_FALSE(ppu.NmiLineLevel());
    CHECK((ppu.CpuRead(0x2002) & 0x80) == 0);

    // The suppression is frame-local: the next frame raises VBlank again.
    ClockDots(ppu, DotsPerFrame + DotsUntilVBlankSetDot + 1 -
                  DotsUntil(241, 100));
    CHECK(ppu.NmiLineLevel());
    CHECK((ppu.CpuRead(0x2002) & 0x80) != 0);
}

TEST_CASE("$2000 NMI enable drives the line level, edges are latched by the CPU")
{
    dendyforge::PPU ppu;

    ClockDots(ppu, DotsUntil(241, 100) + 1);
    CHECK_FALSE(ppu.NmiLineLevel());

    // Move on to the next frame's VBlank with the flag untouched: the
    // flag is set but the enable is still clear, so the line stays low.
    ClockDots(ppu, DotsPerFrame - 1);
    CHECK_FALSE(ppu.NmiLineLevel());

    ppu.CpuWrite(0x2000, 0x80);
    CHECK(ppu.NmiLineLevel());

    // Disabling drops the line; re-enabling raises it again (a new edge
    // for the CPU's detector).
    ppu.CpuWrite(0x2000, 0x00);
    CHECK_FALSE(ppu.NmiLineLevel());
    ppu.CpuWrite(0x2000, 0x80);
    CHECK(ppu.NmiLineLevel());

    // Clearing the flag with $2002 drops the line while enabled.
    ppu.CpuRead(0x2002);
    CHECK_FALSE(ppu.NmiLineLevel());
}

TEST_CASE("A $2002 read during VBlank clears the flag and drops the line")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2000, 0x80);

    ClockDots(ppu, DotsUntil(241, 2) + 1);

    CHECK(ppu.NmiLineLevel());
    CHECK((ppu.CpuRead(0x2002) & 0x80) != 0);
    CHECK_FALSE(ppu.NmiLineLevel());
    CHECK((ppu.CpuRead(0x2002) & 0x80) == 0);
}

namespace
{

void WriteOamSprite(dendyforge::PPU& ppu, std::uint8_t index,
                    std::uint8_t y, std::uint8_t tile,
                    std::uint8_t attributes, std::uint8_t x)
{
    ppu.CpuWrite(0x2003, index * 4);
    ppu.CpuWrite(0x2004, y);
    ppu.CpuWrite(0x2004, tile);
    ppu.CpuWrite(0x2004, attributes);
    ppu.CpuWrite(0x2004, x);
}

void PrepareRenderedPattern(dendyforge::PPU& ppu)
{
    ppu.CpuWrite(0x2001, 0x1E);
    // Background tile 0 and sprite tile 1: every row opaque in column 0
    // (low plane bit 7), high plane clear, so any scanline row works.
    for (int row = 0; row < 8; ++row)
    {
        ppu.PpuWrite(static_cast<std::uint16_t>(row), 0x80);
        ppu.PpuWrite(static_cast<std::uint16_t>(0x0010 + row), 0x80);
    }
    ppu.PpuWrite(0x2000, 0x00);
    ppu.PpuWrite(0x3F00, 0x0F);
    ppu.PpuWrite(0x3F01, 0x21);
    ppu.PpuWrite(0x3F11, 0x31);

    // Unwritten OAM reads as y=0 and would be in range on scanline 0;
    // park every sprite off-screen so tests control exactly which ones
    // are live.
    for (std::uint8_t sprite = 0; sprite < 64; ++sprite)
    {
        WriteOamSprite(ppu, sprite, 240, 0, 0, 0);
    }
}

} // namespace

TEST_CASE("Sprite evaluation sets the overflow flag at the ninth live sprite")
{
    dendyforge::PPU ppu;
    PrepareRenderedPattern(ppu);

    // Nine sprites on scanline 17: eight fill the secondary OAM (one byte
    // per dot pair, eight dots per sprite starting at dot 65), so the
    // ninth is detected at scanline 16, dot 130.
    for (std::uint8_t sprite = 0; sprite < 9; ++sprite)
    {
        WriteOamSprite(ppu, sprite, 16, 0, 0, sprite * 8);
    }

    ClockDots(ppu, DotsUntil(16, 129) + 1);
    CHECK((ppu.CpuRead(0x2002) & 0x20) == 0);

    ClockDots(ppu, 1);
    CHECK((ppu.CpuRead(0x2002) & 0x20) != 0);
}

TEST_CASE("Sprite zero hit latches on the exact dot of pixel output")
{
    dendyforge::PPU ppu;
    PrepareRenderedPattern(ppu);
    WriteOamSprite(ppu, 0, 0, 1, 0, 0);

    ClockDots(ppu, DotsUntil(1, 0) + 1);
    CHECK((ppu.CpuRead(0x2002) & 0x40) == 0);

    ClockDots(ppu, 1);
    CHECK((ppu.CpuRead(0x2002) & 0x40) != 0);
}

TEST_CASE("Sprite zero hit does not trigger at x=255")
{
    dendyforge::PPU ppu;
    PrepareRenderedPattern(ppu);
    WriteOamSprite(ppu, 0, 0, 1, 0, 255);

    // Past the end of scanline 1's pixel output: the opaque overlap at
    // x=255 must not latch the hit.
    ClockDots(ppu, DotsUntil(1, 256) + 1);
    CHECK((ppu.CpuRead(0x2002) & 0x40) == 0);
}

TEST_CASE("OAMADDR is held at zero during the sprite fetch interval")
{
    dendyforge::PPU ppu;
    PrepareRenderedPattern(ppu);
    ppu.CpuWrite(0x2003, 0x40);

    ClockDots(ppu, DotsUntil(0, 258) + 1);

    // The $2004 write must land at OAM[0], not OAM[0x40].
    ppu.CpuWrite(0x2004, 0xAB);
    ppu.CpuWrite(0x2003, 0x00);
    CHECK(ppu.CpuRead(0x2004) == 0xAB);
}

TEST_CASE("A behind-background sprite hides later sprites at the same pixel")
{
    dendyforge::PPU backgroundOnly;
    PrepareRenderedPattern(backgroundOnly);

    dendyforge::PPU withShadows;
    PrepareRenderedPattern(withShadows);
    // Sprite 0 sits behind the background, sprite 1 in front; both opaque
    // at x=0. The multiplexer selects sprite 0, which loses to the
    // background, so sprite 1 must not show either.
    WriteOamSprite(withShadows, 0, 0, 1, 0x20, 0);
    WriteOamSprite(withShadows, 1, 0, 1, 0x00, 0);

    backgroundOnly.RenderBackground();
    withShadows.RenderBackground();
    withShadows.RenderSprites();

    CHECK(withShadows.FrameBuffer()[256] == backgroundOnly.FrameBuffer()[256]);
}

TEST_CASE("PPU register accesses share one open-bus latch")
{
    dendyforge::PPU ppu;

    // A write drives the value onto the bus; $2002 echoes its low bits.
    ppu.CpuWrite(0x2001, 0x2B);
    CHECK((ppu.CpuRead(0x2002) & 0x1F) == 0x0B);

    // Reading a write-only register returns the latch unchanged.
    CHECK(ppu.CpuRead(0x2000) == 0x0B);
    CHECK(ppu.CpuRead(0x2005) == 0x0B);

    // A $2007 palette read drives the palette value onto the bus.
    ppu.PpuWrite(0x3F00, 0x1F);
    ppu.CpuWrite(0x2006, 0x3F);
    ppu.CpuWrite(0x2006, 0x00);
    for (int dot = 0; dot < 3; ++dot)
    {
        ppu.Clock();
    }
    CHECK(ppu.CpuRead(0x2007) == 0x1F);
    CHECK(ppu.CpuRead(0x2000) == 0x1F);

    // A $2002 read itself latches the returned status byte.
    const std::uint8_t status = ppu.CpuRead(0x2002);
    CHECK(ppu.CpuRead(0x2000) == status);
}
