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
    CHECK(ppu.ConsumeFrameComplete());
    CHECK_FALSE(ppu.ConsumeFrameComplete());
    CHECK((ppu.CpuRead(0x2002) & 0x80) != 0);
}

TEST_CASE("PPU scroll and address writes keep v, t, fine X, and the latch distinct")
{
    dendyforge::PPU ppu;

    ppu.CpuWrite(0x2000, 0x03);
    ppu.CpuWrite(0x2005, 0x1D);
    ppu.CpuWrite(0x2005, 0xAE);

    const auto afterScroll = ppu.AddressState();
    CHECK(afterScroll.vramAddress == 0x0000);
    CHECK(afterScroll.temporaryAddress == 0x6EA3);
    CHECK(afterScroll.fineX == 0x05);
    CHECK_FALSE(afterScroll.writeLatch);

    ppu.CpuWrite(0x2006, 0x3F);
    const auto afterAddressHigh = ppu.AddressState();
    CHECK(afterAddressHigh.vramAddress == 0x0000);
    CHECK(afterAddressHigh.temporaryAddress == 0x3FA3);
    CHECK(afterAddressHigh.writeLatch);

    ppu.CpuRead(0x2002);
    CHECK_FALSE(ppu.AddressState().writeLatch);

    ppu.CpuWrite(0x2006, 0x21);
    ppu.CpuWrite(0x2006, 0x00);
    const auto afterAddress = ppu.AddressState();
    CHECK(afterAddress.vramAddress == 0x2100);
    CHECK(afterAddress.temporaryAddress == 0x2100);
    CHECK(afterAddress.fineX == 0x05);
    CHECK_FALSE(afterAddress.writeLatch);
}

TEST_CASE("PPUSCROLL prepares t without moving the PPUDATA address")
{
    dendyforge::PPU ppu;
    ppu.PpuWrite(0x2015, 0xAA);
    ppu.PpuWrite(0x2016, 0xBB);

    ppu.CpuWrite(0x2006, 0x20);
    ppu.CpuWrite(0x2006, 0x15);
    CHECK(ppu.CpuRead(0x2007) == 0x00);
    CHECK(ppu.AddressState().vramAddress == 0x2016);

    ppu.CpuWrite(0x2005, 0x1D);
    ppu.CpuWrite(0x2005, 0xAE);
    CHECK(ppu.AddressState().vramAddress == 0x2016);

    CHECK(ppu.CpuRead(0x2007) == 0xAA);
    CHECK(ppu.CpuRead(0x2007) == 0xBB);
}

TEST_CASE("PPU renders a background tile into the frame buffer")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2001, 0x0A);

    ppu.PpuWrite(0x0000, 0x80);
    ppu.PpuWrite(0x0008, 0x00);
    ppu.PpuWrite(0x2000, 0x00);
    ppu.PpuWrite(0x23C0, 0x00);
    ppu.PpuWrite(0x3F00, 0x0F);
    ppu.PpuWrite(0x3F01, 0x21);

    ppu.RenderBackground();

    CHECK(ppu.FrameBuffer()[0] != ppu.FrameBuffer()[1]);
    CHECK(ppu.FrameBuffer()[1] == ppu.FrameBuffer()[256]);
}

TEST_CASE("PPU background renderer applies scroll from the PPU scroll register")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2001, 0x0A);
    ppu.PpuWrite(0x0000, 0x80);
    ppu.PpuWrite(0x0010, 0x80);
    ppu.PpuWrite(0x0018, 0x80);
    ppu.PpuWrite(0x2000, 0x00);
    ppu.PpuWrite(0x2001, 0x01);
    ppu.PpuWrite(0x3F00, 0x0F);
    ppu.PpuWrite(0x3F01, 0x21);
    ppu.PpuWrite(0x3F03, 0x31);

    ppu.RenderBackground();
    const auto beforeScroll = ppu.FrameBuffer()[0];

    ppu.CpuWrite(0x2005, 8);
    ppu.CpuWrite(0x2005, 0);
    ppu.RenderBackground();

    CHECK(ppu.FrameBuffer()[0] != beforeScroll);
}

TEST_CASE("PPU background renderer applies fine horizontal scroll to pattern bits")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2001, 0x0A);
    ppu.PpuWrite(0x0000, 0x80);
    ppu.PpuWrite(0x0008, 0x00);
    ppu.PpuWrite(0x2000, 0x00);
    ppu.PpuWrite(0x3F00, 0x0F);
    ppu.PpuWrite(0x3F01, 0x21);

    ppu.RenderBackground();
    const auto unscrolledPixel = ppu.FrameBuffer()[0];

    ppu.CpuWrite(0x2005, 0x01);
    ppu.CpuWrite(0x2005, 0x00);
    ppu.RenderBackground();

    CHECK(ppu.FrameBuffer()[0] != unscrolledPixel);
    CHECK(ppu.FrameBuffer()[0] == ppu.FrameBuffer()[1]);
}

TEST_CASE("PPU rendering copies t into v and advances coarse X at fine-X boundaries")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2000, 0x03);
    ppu.CpuWrite(0x2001, 0x08);
    ppu.CpuWrite(0x2005, 0x1D);
    ppu.CpuWrite(0x2005, 0xAE);

    for (int cycle = 0; cycle < 341; ++cycle)
    {
        ppu.Clock();
    }
    CHECK(ppu.AddressState().vramAddress == 0x6EA5);

    for (int cycle = 0; cycle < 4; ++cycle)
    {
        ppu.Clock();
    }
    CHECK(ppu.AddressState().vramAddress == 0x6EA5);
}

TEST_CASE("PPU coarse X progression switches the horizontal nametable")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2001, 0x08);
    ppu.CpuWrite(0x2005, 0xF8);
    ppu.CpuWrite(0x2005, 0x00);

    for (int cycle = 0; cycle < 341; ++cycle)
    {
        ppu.Clock();
    }
    CHECK(ppu.AddressState().vramAddress == 0x0401);

    for (int cycle = 0; cycle < 9; ++cycle)
    {
        ppu.Clock();
    }
    CHECK(ppu.AddressState().vramAddress == 0x0402);
}

TEST_CASE("PPU rendering advances Y at cycle 256 and restores horizontal t bits")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2001, 0x08);

    for (int cycle = 0; cycle < 341 + 257; ++cycle)
    {
        ppu.Clock();
    }
    CHECK(ppu.AddressState().vramAddress == 0x1402);

    ppu.Clock();
    CHECK(ppu.AddressState().vramAddress == 0x1000);
}

TEST_CASE("PPU renders an OAM sprite and records a Sprite Zero Hit")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2001, 0x1E);
    ppu.PpuWrite(0x0000, 0x80);
    ppu.PpuWrite(0x0001, 0x80);
    ppu.PpuWrite(0x0008, 0x00);
    ppu.PpuWrite(0x0010, 0x80);
    ppu.PpuWrite(0x0018, 0x00);
    ppu.PpuWrite(0x2000, 0x00);
    ppu.PpuWrite(0x3F01, 0x21);
    ppu.PpuWrite(0x3F11, 0x31);

    ppu.CpuWrite(0x2003, 0x00);
    ppu.CpuWrite(0x2004, 0x00);
    ppu.CpuWrite(0x2004, 0x01);
    ppu.CpuWrite(0x2004, 0x00);
    ppu.CpuWrite(0x2004, 0x00);

    ppu.RenderBackground();
    const auto backgroundPixel = ppu.FrameBuffer()[256];
    ppu.RenderSprites();

    CHECK(ppu.FrameBuffer()[256] != backgroundPixel);
    CHECK((ppu.CpuRead(0x2002) & 0x40) != 0);
}

TEST_CASE("PPU fetches sprites for the next scanline before rendering it")
{
    dendyforge::PPU backgroundOnly;
    dendyforge::PPU withSprite;

    for (dendyforge::PPU* ppu : {&backgroundOnly, &withSprite})
    {
        ppu->CpuWrite(0x2001, 0x1E);
        ppu->PpuWrite(0x0001, 0x80);
        ppu->PpuWrite(0x0010, 0x80);
        ppu->PpuWrite(0x0018, 0x00);
        ppu->PpuWrite(0x2000, 0x00);
        ppu->PpuWrite(0x3F00, 0x0F);
        ppu->PpuWrite(0x3F01, 0x21);
        ppu->PpuWrite(0x3F11, 0x31);
    }

    withSprite.CpuWrite(0x2003, 0x00);
    withSprite.CpuWrite(0x2004, 0x00);
    withSprite.CpuWrite(0x2004, 0x01);
    withSprite.CpuWrite(0x2004, 0x00);
    withSprite.CpuWrite(0x2004, 0x00);

    for (int cycle = 0; cycle < 684; ++cycle)
    {
        backgroundOnly.Clock();
        withSprite.Clock();
    }

    CHECK(withSprite.FrameBuffer()[256] != backgroundOnly.FrameBuffer()[256]);
    CHECK((withSprite.CpuRead(0x2002) & 0x40) != 0);
}

TEST_CASE("PPU applies the eight-sprite scanline limit and 8x16 sprite mode")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2000, 0x20);
    ppu.CpuWrite(0x2001, 0x14);
    ppu.PpuWrite(0x0000, 0x80);
    ppu.PpuWrite(0x0010, 0x80);
    ppu.PpuWrite(0x3F11, 0x21);

    ppu.CpuWrite(0x2003, 0x00);
    for (int sprite = 0; sprite < 9; ++sprite)
    {
        ppu.CpuWrite(0x2004, 0x00);
        ppu.CpuWrite(0x2004, 0x00);
        ppu.CpuWrite(0x2004, 0x00);
        ppu.CpuWrite(0x2004, sprite * 8);
    }

    ppu.RenderBackground();
    ppu.RenderSprites();

    CHECK((ppu.CpuRead(0x2002) & 0x20) != 0);
    CHECK(ppu.FrameBuffer()[256] != ppu.FrameBuffer()[256 + 64]);
    CHECK(ppu.FrameBuffer()[256 + 64] == ppu.FrameBuffer()[256 + 72]);
}

TEST_CASE("PPU applies PPUMASK clipping, grayscale, and color emphasis")
{
    dendyforge::PPU ppu;
    ppu.PpuWrite(0x0000, 0xC0);
    ppu.PpuWrite(0x0008, 0x00);
    ppu.PpuWrite(0x2000, 0x00);
    ppu.PpuWrite(0x3F00, 0x0F);
    ppu.PpuWrite(0x3F01, 0x21);

    ppu.CpuWrite(0x2001, 0x08);
    ppu.RenderBackground();
    const auto clippedPixel = ppu.FrameBuffer()[0];
    const auto visiblePixel = ppu.FrameBuffer()[8];
    CHECK(clippedPixel != visiblePixel);

    ppu.CpuWrite(0x2001, 0x0B);
    ppu.RenderBackground();
    const auto grayscalePixel = ppu.FrameBuffer()[8];
    CHECK(grayscalePixel != visiblePixel);

    ppu.CpuWrite(0x2001, 0x2A);
    ppu.RenderBackground();
    CHECK(ppu.FrameBuffer()[8] != grayscalePixel);
}

TEST_CASE("PPU renders visible scanlines before VBlank")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2001, 0x0A);
    ppu.PpuWrite(0x0000, 0x80);
    ppu.PpuWrite(0x0008, 0x00);
    ppu.PpuWrite(0x2000, 0x00);
    ppu.PpuWrite(0x3F00, 0x0F);
    ppu.PpuWrite(0x3F01, 0x21);

    for (int cycle = 0; cycle < 341 + 257; ++cycle)
    {
        ppu.Clock();
    }

    CHECK(ppu.FrameBuffer()[0] != ppu.FrameBuffer()[1]);
    CHECK_FALSE(ppu.PollNmi());
}

TEST_CASE("PPU emits background pixels one PPU clock at a time")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2001, 0x0A);
    ppu.PpuWrite(0x0000, 0x80);
    ppu.PpuWrite(0x0008, 0x00);
    ppu.PpuWrite(0x2000, 0x00);
    ppu.PpuWrite(0x3F00, 0x0F);
    ppu.PpuWrite(0x3F01, 0x21);

    for (int cycle = 0; cycle < 341 + 2; ++cycle)
    {
        ppu.Clock();
    }

    CHECK(ppu.FrameBuffer()[0] != ppu.FrameBuffer()[1]);
}

TEST_CASE("PPU background shifters preserve tile boundaries during clocked rendering")
{
    dendyforge::PPU ppu;
    ppu.CpuWrite(0x2001, 0x0A);
    ppu.PpuWrite(0x0000, 0x80);
    ppu.PpuWrite(0x0010, 0x80);
    ppu.PpuWrite(0x0008, 0x00);
    ppu.PpuWrite(0x0018, 0x00);
    ppu.PpuWrite(0x2000, 0x00);
    ppu.PpuWrite(0x2001, 0x01);
    ppu.PpuWrite(0x3F00, 0x0F);
    ppu.PpuWrite(0x3F01, 0x21);

    for (int cycle = 0; cycle < 341 + 10; ++cycle)
    {
        ppu.Clock();
    }

    CHECK(ppu.FrameBuffer()[0] != ppu.FrameBuffer()[1]);
    CHECK(ppu.FrameBuffer()[8] == ppu.FrameBuffer()[0]);
}

TEST_CASE("PPU background shifters select the fine-X pattern bit")
{
    dendyforge::PPU unscrolled;
    unscrolled.CpuWrite(0x2001, 0x0A);
    unscrolled.PpuWrite(0x0000, 0x80);
    unscrolled.PpuWrite(0x0008, 0x00);
    unscrolled.PpuWrite(0x2000, 0x00);
    unscrolled.PpuWrite(0x3F00, 0x0F);
    unscrolled.PpuWrite(0x3F01, 0x21);

    dendyforge::PPU fineScrolled;
    fineScrolled.CpuWrite(0x2001, 0x0A);
    fineScrolled.PpuWrite(0x0000, 0x80);
    fineScrolled.PpuWrite(0x0008, 0x00);
    fineScrolled.PpuWrite(0x2000, 0x00);
    fineScrolled.PpuWrite(0x3F00, 0x0F);
    fineScrolled.PpuWrite(0x3F01, 0x21);
    fineScrolled.CpuWrite(0x2005, 0x01);
    fineScrolled.CpuWrite(0x2005, 0x00);

    for (int cycle = 0; cycle < 341 + 2; ++cycle)
    {
        unscrolled.Clock();
        fineScrolled.Clock();
    }

    CHECK(unscrolled.FrameBuffer()[0] != unscrolled.FrameBuffer()[1]);
    CHECK(fineScrolled.FrameBuffer()[0] == fineScrolled.FrameBuffer()[1]);
}

TEST_CASE("PPU background attribute shifters select the fetched palette")
{
    dendyforge::PPU firstPalette;
    firstPalette.CpuWrite(0x2001, 0x0A);
    firstPalette.PpuWrite(0x0000, 0x80);
    firstPalette.PpuWrite(0x0008, 0x00);
    firstPalette.PpuWrite(0x2000, 0x00);
    firstPalette.PpuWrite(0x23C0, 0x00);
    firstPalette.PpuWrite(0x3F00, 0x0F);
    firstPalette.PpuWrite(0x3F01, 0x21);
    firstPalette.PpuWrite(0x3F09, 0x31);

    dendyforge::PPU thirdPalette;
    thirdPalette.CpuWrite(0x2001, 0x0A);
    thirdPalette.PpuWrite(0x0000, 0x80);
    thirdPalette.PpuWrite(0x0008, 0x00);
    thirdPalette.PpuWrite(0x2000, 0x00);
    thirdPalette.PpuWrite(0x23C0, 0x02);
    thirdPalette.PpuWrite(0x3F00, 0x0F);
    thirdPalette.PpuWrite(0x3F01, 0x21);
    thirdPalette.PpuWrite(0x3F09, 0x31);

    for (int cycle = 0; cycle < 341 + 2; ++cycle)
    {
        firstPalette.Clock();
        thirdPalette.Clock();
    }

    CHECK(firstPalette.FrameBuffer()[0] != thirdPalette.FrameBuffer()[0]);
}
