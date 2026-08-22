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

TEST_CASE("VBlank sets at scanline 241 cycle 1 and latches exactly one NMI")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2000, 0x80);

    ClockDots(ppu, DotsUntilVBlankSetDot + 1);

    CHECK(ppu.PollNmi());
    CHECK_FALSE(ppu.PollNmi());
    CHECK((ppu.CpuRead(0x2002) & 0x80) != 0);
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
    CHECK_FALSE(ppu.PollNmi());
    CHECK((ppu.CpuRead(0x2002) & 0x80) == 0);

    // The suppression is frame-local: the next frame raises VBlank again.
    ClockDots(ppu, DotsPerFrame + DotsUntilVBlankSetDot + 1 -
                  DotsUntil(241, 100));
    CHECK(ppu.PollNmi());
    CHECK((ppu.CpuRead(0x2002) & 0x80) != 0);
}

TEST_CASE("$2000 NMI enable follows the NMI line, one NMI per rising edge")
{
    dendyforge::PPU ppu;

    ClockDots(ppu, DotsUntil(241, 100) + 1);
    CHECK_FALSE(ppu.PollNmi());

    // Enabling NMI while the VBlank flag is set raises the line: one NMI.
    ppu.CpuWrite(0x2000, 0x80);
    CHECK(ppu.PollNmi());
    CHECK_FALSE(ppu.PollNmi());

    // Rewriting the same value must not retrigger.
    ppu.CpuWrite(0x2000, 0x80);
    CHECK_FALSE(ppu.PollNmi());

    // Disabling, then re-enabling inside the same VBlank is a new edge.
    ppu.CpuWrite(0x2000, 0x00);
    CHECK_FALSE(ppu.PollNmi());
    ppu.CpuWrite(0x2000, 0x80);
    CHECK(ppu.PollNmi());
}

TEST_CASE("A $2002 read during VBlank clears the flag and the pending NMI")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2000, 0x80);

    ClockDots(ppu, DotsUntil(241, 2) + 1);

    CHECK((ppu.CpuRead(0x2002) & 0x80) != 0);
    CHECK((ppu.CpuRead(0x2002) & 0x80) == 0);
    CHECK_FALSE(ppu.PollNmi());
}
