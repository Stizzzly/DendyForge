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
    // OAM writes are ignored while rendering, exactly as on hardware, so
    // drop $2001 for the duration of the setup writes like a game does in
    // VBlank and restore the caller's mask afterwards.
    const std::uint8_t mask = ppu.DebugPeekCpuRegister(0x2001).value_or(0);
    ppu.CpuWrite(0x2001, 0x00);
    ppu.CpuWrite(0x2003, index * 4);
    ppu.CpuWrite(0x2004, y);
    ppu.CpuWrite(0x2004, tile);
    ppu.CpuWrite(0x2004, attributes);
    ppu.CpuWrite(0x2004, x);
    ppu.CpuWrite(0x2001, mask);
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

TEST_CASE("PPUSTATUS samples sprite flags at the end of the CPU read")
{
    const auto readAtPreRenderDot = [](std::int16_t cycle)
    {
        dendyforge::PPU ppu;
        PrepareRenderedPattern(ppu);
        for (std::uint8_t sprite = 0; sprite < 9; ++sprite)
        {
            WriteOamSprite(ppu, sprite, 0, 1, 0, sprite * 8);
        }

        // Produce sprite zero hit and overflow, leave VBlank unread, then
        // stop with the selected pre-render dot pending.
        ClockDots(ppu, DotsUntil(1, 130) + 1);
        CHECK((ppu.DebugPeekCpuRegister(0x2002).value_or(0) & 0x60) == 0x60);
        do
        {
            ppu.Clock();
        }
        while (ppu.Scanline() != -1 || ppu.Cycle() != cycle);

        return static_cast<std::uint8_t>(ppu.CpuRead(0x2002) & 0xE0);
    };

    // Dot 0 does not reach the clear before this scheduler's M2-low sample;
    // a read beginning on dot 1 does. Both still latch the old VBlank bit.
    CHECK(readAtPreRenderDot(0) == 0xE0);
    CHECK(readAtPreRenderDot(1) == 0x80);
}

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

TEST_CASE("Sprite size changes affect only pattern fetches that have not completed")
{
    auto runResize = [](std::int16_t resizeCycle)
    {
        dendyforge::PPU ppu;
        PrepareRenderedPattern(ppu);

        // Keep the background transparent until the one target overlap at
        // scanline 16, x=16, so an earlier sprite-zero hit cannot remain
        // latched and mask the fetch-boundary result.
        for (int row = 0; row < 8; ++row)
        {
            ppu.PpuWrite(static_cast<std::uint16_t>(row), 0x00);
            ppu.PpuWrite(static_cast<std::uint16_t>(0x0020 + row), 0x80);
        }
        ppu.PpuWrite(0x2042, 0x02);

        // In 8x16 mode this sprite is still in range while scanline 15 is
        // preparing scanline 16 (row 15, tile 1). In 8x8 mode it is not.
        WriteOamSprite(ppu, 0, 0, 0, 0, 16);
        ppu.CpuWrite(0x2000, 0x20);
        ppu.CpuWrite(0x2001, 0x1E);

        ClockDots(ppu, DotsUntil(15, resizeCycle));
        ppu.CpuWrite(0x2000, 0x00);
        ClockDots(ppu, DotsUntil(16, 17) - DotsUntil(15, resizeCycle) + 1);
        return (ppu.CpuRead(0x2002) & 0x40) != 0;
    };

    // Slot zero loads its low/high pattern bytes at dots 261 and 263.
    // Resizing before them must discard the previously loaded scanline's
    // planes, while resizing afterwards must not alter the loaded shifters.
    CHECK_FALSE(runResize(257));
    CHECK(runResize(265));
}

TEST_CASE("OAMADDR is held at zero during the sprite fetch interval")
{
    dendyforge::PPU ppu;
    PrepareRenderedPattern(ppu);
    ppu.CpuWrite(0x2003, 0x40);

    ClockDots(ppu, DotsUntil(0, 258) + 1);

    CHECK(ppu.DebugPeekCpuRegister(0x2003) == 0x00);

    // A $2004 write while rendering does not reach OAM; it only performs
    // the glitchy increment of the high six OAMADDR bits.
    ppu.CpuWrite(0x2004, 0xAB);
    CHECK(ppu.DebugPeekCpuRegister(0x2003) == 0x04);
    ppu.CpuWrite(0x2001, 0x00);
    ppu.CpuWrite(0x2003, 0x00);
    CHECK(ppu.CpuRead(0x2004) != 0xAB);
}

TEST_CASE("OAMDATA exposes the secondary OAM fetch buffer")
{
    dendyforge::PPU ppu;
    PrepareRenderedPattern(ppu);
    WriteOamSprite(ppu, 0, 0x00, 0x12, 0x03, 0x34);

    ClockDots(ppu, DotsUntil(0, 257));

    // Each eight-dot sprite fetch exposes Y, tile and attributes once, then
    // holds X for the remaining five dots. At dot 321 the secondary-OAM
    // address wraps to the first byte while background fetches resume.
    ppu.Clock();
    CHECK(ppu.CpuRead(0x2004) == 0x00);
    ppu.Clock();
    CHECK(ppu.CpuRead(0x2004) == 0x12);
    ppu.Clock();
    CHECK(ppu.CpuRead(0x2004) == 0x03);
    ppu.Clock();
    CHECK(ppu.CpuRead(0x2004) == 0x34);
    ClockDots(ppu, 4);
    CHECK(ppu.CpuRead(0x2004) == 0x34);
    ClockDots(ppu, 10);
    CHECK(ppu.CpuRead(0x2004) == 0xFF);
    ppu.Clock();
    // An empty secondary-OAM attribute slot still drives all eight bits.
    CHECK(ppu.CpuRead(0x2004) == 0xFF);
    ClockDots(ppu, 45);
    CHECK(ppu.CpuRead(0x2004) == 0xFF);
    ppu.Clock();
    CHECK(ppu.CpuRead(0x2004) == 0x00);
}

TEST_CASE("OAM evaluation holds the terminal primary byte after OAM wraps")
{
    dendyforge::PPU ppu;
    PrepareRenderedPattern(ppu);
    WriteOamSprite(ppu, 0, 0x00, 0x12, 0x03, 0x34);

    // One in-range entry takes dots 65-72. The remaining 63 Y checks take
    // two dots each, wrapping primary OAM at dot 198. Dot 199 reads the
    // wrapped primary byte; dot 200 is the corresponding secondary-OAM
    // read and exposes the terminal primary-OAM value that caused the wrap.
    ClockDots(ppu, DotsUntil(0, 199));
    ppu.Clock();
    CHECK(ppu.CpuRead(0x2004) == 0x00);
    ppu.Clock();
    CHECK(ppu.CpuRead(0x2004) == 0xF0);
}

TEST_CASE("Sprite overflow evaluation realigns after fetching an extra sprite")
{
    dendyforge::PPU ppu;

    ppu.CpuWrite(0x2003, 0x00);
    for (int address = 0; address < 256; ++address)
    {
        ppu.CpuWrite(0x2004, 0xF0);
    }
    for (std::uint8_t sprite = 0; sprite < 8; ++sprite)
    {
        WriteOamSprite(ppu, sprite, 0x00, 0x00, 0x00, 0x00);
    }

    // Once secondary OAM is full, the failed Y check at OAM[32] advances
    // diagonally to OAM[37]. Treat that byte as an in-range Y, finish its
    // remaining bytes, then realign at OAM[40] for the failed-copy phase.
    ppu.CpuWrite(0x2003, 37);
    ppu.CpuWrite(0x2004, 0x00);
    ppu.CpuWrite(0x2003, 40);
    ppu.CpuWrite(0x2004, 0x22);
    ppu.CpuWrite(0x2003, 45);
    ppu.CpuWrite(0x2004, 0x55);
    ppu.CpuWrite(0x2001, 0x10);

    ClockDots(ppu, DotsUntil(0, 137));
    ppu.Clock();
    CHECK(ppu.CpuRead(0x2004) == 0x22);
    ppu.Clock(); // secondary-OAM phase
    ppu.Clock();
    CHECK(ppu.CpuRead(0x2004) == 0x22);
}

TEST_CASE("OAM overflow stress window keeps secondary byte zero on even phases")
{
    dendyforge::PPU ppu;
    constexpr std::uint8_t firstSprites[32]{
        0x80, 0x00, 0x00, 0xFF, 0x7F, 0x01, 0x20, 0xEE,
        0x7E, 0x02, 0x40, 0xDD, 0x7D, 0x03, 0x60, 0xCC,
        0x7C, 0x04, 0x80, 0xBB, 0x7B, 0x05, 0xA0, 0xAA,
        0x7A, 0x06, 0xC0, 0x99, 0x79, 0x07, 0xE0, 0x88,
    };

    ppu.CpuWrite(0x2003, 0x00);
    for (int address = 0; address < 256; ++address)
    {
        const std::uint8_t value = address < 32
            ? firstSprites[address]
            : static_cast<std::uint8_t>(address - 32);
        ppu.CpuWrite(0x2004, value);
    }
    ppu.CpuWrite(0x2001, 0x10);
    ClockDots(ppu, DotsUntil(128, 0));

    std::array<std::uint8_t, 0xCA> observed{};
    for (auto& value : observed)
    {
        ppu.Clock();
        value = ppu.CpuRead(0x2004);
    }

    constexpr std::uint8_t expected[] = {
        0x80, 0x62, 0x80, 0x7F, 0x80, 0x80, 0x80,
        0x81, 0x80, 0x82, 0x80, 0x80, 0x80, 0x84,
    };
    for (std::size_t index = 0; index < std::size(expected); ++index)
    {
        CAPTURE(index);
        CHECK(observed[0xBC + index] == expected[index]);
    }
}

TEST_CASE("Re-enabling rendering applies delayed OAM row corruption")
{
    dendyforge::PPU ppu;

    // Row zero is the source marker; every other row starts distinct.
    ppu.CpuWrite(0x2003, 0x00);
    for (int byte = 0; byte < 256; ++byte)
    {
        ppu.CpuWrite(0x2004, 0xFF);
    }
    ppu.CpuWrite(0x2003, 0x00);
    ppu.CpuWrite(0x2004, 0x00);

    ppu.CpuWrite(0x2001, 0x10);
    ClockDots(ppu, DotsUntil(0, 7));

    // With the four-dot PPUMASK propagation delay, this transition is
    // observed at dot 10 and seeds primary-OAM row five.
    ppu.CpuWrite(0x2001, 0x00);
    ClockDots(ppu, 4);
    ppu.CpuWrite(0x2001, 0x10);
    ClockDots(ppu, 4);

    // Drop rendering again so $2004 exposes primary OAM normally.
    ppu.CpuWrite(0x2001, 0x00);
    ClockDots(ppu, 4);
    ppu.CpuWrite(0x2003, 5 * 8);
    CHECK(ppu.CpuRead(0x2004) == 0x00);
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
    // Bit 0 (grayscale) stays clear so palette reads are not masked, and
    // bits 3-4 stay clear so rendering remains disabled.
    ppu.CpuWrite(0x2001, 0x26);
    CHECK((ppu.CpuRead(0x2002) & 0x1F) == 0x06);

    // Reading a write-only register returns the latch unchanged.
    CHECK(ppu.CpuRead(0x2000) == 0x06);
    CHECK(ppu.CpuRead(0x2005) == 0x06);

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

TEST_CASE("PPUMASK rendering enables reach the pipeline after four dot clocks")
{
    dendyforge::PPU enabling;
    ClockDots(enabling, 6); // pre-render dot 6 is next
    enabling.CpuWrite(0x2001, 0x08);

    // The would-be tile load at dot 8 remains disabled because only three
    // dot clocks have elapsed since the write.
    ClockDots(enabling, 3);
    CHECK((enabling.AddressState().vramAddress & 0x001F) == 0);

    dendyforge::PPU disabling;
    disabling.CpuWrite(0x2001, 0x08);
    ClockDots(disabling, 6); // enable has applied; dot 6 is next
    disabling.CpuWrite(0x2001, 0x00);

    // Conversely, the old enabled state survives through dot 8 and lets its
    // shifter load/coarse-X increment complete before disable takes effect.
    ClockDots(disabling, 3);
    CHECK((disabling.AddressState().vramAddress & 0x001F) == 1);
}

TEST_CASE("PPUDATA accesses clock both scroll counters during rendering")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2006, 0x20);
    ppu.CpuWrite(0x2006, 0x00);
    ppu.CpuWrite(0x2001, 0x08);
    ClockDots(ppu, 4);

    // From v=$2000 the hardware clocks coarse X to $2001 and fine Y to
    // $3001 instead of applying the normal +1.
    ppu.CpuRead(0x2007);
    ClockDots(ppu, 4);
    CHECK(ppu.AddressState().vramAddress == 0x3001);
}

TEST_CASE("PPUDATA rendering reads fill the buffer from the rendering bus")
{
    dendyforge::PPU ppu;
    ppu.PpuWrite(0x2001, 0x42);
    ppu.CpuWrite(0x2006, 0x20);
    ppu.CpuWrite(0x2006, 0x00);
    ppu.CpuWrite(0x2001, 0x08);

    // Rendering becomes active at dot 3. A CPU read beginning at dot 6
    // completes after the cadence; dot 8 advances coarse X and dot 9
    // fetches nametable byte $2001 onto the multiplexed bus.
    ClockDots(ppu, 6);
    CHECK(ppu.CpuRead(0x2007) == 0x00);
    ClockDots(ppu, 4);
    CHECK(ppu.DebugPeekCpuRegister(0x2007) == 0x42);
}
