#include "ppu.hpp"

#include "cartridge/cartridge.hpp"

namespace dendyforge
{

namespace
{

constexpr std::array<std::uint32_t, 64> SystemPalette{
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00,
    0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000,
    0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
    0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000,
    0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22,
    0xBCBE00, 0x88D800, 0x5CE430, 0x45E082, 0x48CDDE, 0x4F4F4F, 0x000000, 0x000000,
    0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5,
    0xE4E594, 0xCFEE96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000,
};

}

void PPU::ConnectCartridge(Cartridge* cartridge)
{
    m_cartridge = cartridge;
    m_mirroring = cartridge ? cartridge->Info().MirroringMode()
                            : Mirroring::Horizontal;
}

void PPU::Clock()
{
    if (m_scanline == -1 && m_cycle == 1)
    {
        m_status &= ~0xE0;
        m_nmiPending = false;
        UpdateNmiOutput();
        BeginFrame();
    }

    const bool renderingScanline = m_scanline >= -1 && m_scanline < 240;
    const bool backgroundFetchCycle =
        (m_cycle >= 1 && m_cycle <= 256) ||
        (m_cycle >= 321 && m_cycle <= 336);

    if (m_scanline >= 0 && m_scanline < 240 &&
        m_cycle >= 1 && m_cycle <= 256)
    {
        RenderBackgroundPixel(m_scanline, m_cycle - 1);
        RenderSpritePixel(m_scanline, m_cycle - 1);
    }

    if (renderingScanline && RenderingEnabled())
    {
        if (backgroundFetchCycle)
        {
            ClockBackgroundFetch();
        }
        if (m_cycle == 256)
        {
            IncrementY(m_vramAddress);
        }
        if (m_cycle == 257)
        {
            CopyHorizontalBits(m_vramAddress, m_temporaryAddress);
        }
        if (m_scanline == -1 && m_cycle >= 280 && m_cycle <= 304)
        {
            CopyVerticalBits(m_vramAddress, m_temporaryAddress);
        }

        if (m_cycle >= 257 && m_cycle <= 320)
        {
            // Sprite tile loading interval: OAMADDR is held at zero, and
            // each of the eight slots consumes eight dots (garbage
            // nametable fetch, garbage attribute fetch, then the two
            // pattern bytes at (cycle - 257) & 7 == 4).
            m_oamAddress = 0;
            if (m_cycle == 257)
            {
                m_spriteFetchIndex = 0;
                if (m_scanline == -1)
                {
                    // No evaluation ran on the pre-render line, so sprites
                    // are never displayed on scanline 0.
                    m_scanlineSpriteCount = 0;
                }
            }
            switch ((m_cycle - 257) & 0x07)
            {
            case 0:
                PpuRead(0x2000 | (m_vramAddress & 0x0FFF));
                break;
            case 2:
                PpuRead(0x23C0 |
                        (m_vramAddress & 0x0C00) |
                        ((m_vramAddress >> 4) & 0x0038) |
                        ((m_vramAddress >> 2) & 0x0007));
                break;
            case 4:
                LoadSpriteTileInfo();
                break;
            default:
                break;
            }
        }

        if (m_cycle == 337 || m_cycle == 339)
        {
            // The two dummy nametable fetches that end the scanline.
            PpuRead(0x2000 | (m_vramAddress & 0x0FFF));
        }
    }

    // Secondary OAM clear and sprite evaluation run on visible scanlines
    // only; the selected sprites are rendered on the following scanline.
    if (m_scanline >= 0 && m_scanline < 240 && RenderingEnabled() &&
        m_cycle >= 1 && m_cycle <= 256)
    {
        ProcessSpriteEvaluation();
    }

    if (m_scanline == 241 && m_cycle == 1)
    {
        // A $2002 read that landed on the dot before this one suppresses
        // the flag and the NMI for this frame only.
        if (m_suppressVblank)
        {
            m_suppressVblank = false;
        }
        else
        {
            m_status |= 0x80;
            UpdateNmiOutput();
        }
        m_frameComplete = true;
    }

    ++m_cycle;
    if (m_scanline == -1 && m_cycle == 340 && m_oddFrame &&
        RenderingEnabled())
    {
        m_cycle = 0;
        m_scanline = 0;
        return;
    }
    if (m_cycle < 341)
    {
        return;
    }

    m_cycle = 0;
    ++m_scanline;
    if (m_scanline == 261)
    {
        m_scanline = -1;
        m_oddFrame = !m_oddFrame;
    }
}

const std::array<std::uint32_t, 256 * 240>& PPU::FrameBuffer() const
{
    return m_frameBuffer;
}

PPU::ScrollAddressState PPU::AddressState() const
{
    return {m_vramAddress, m_temporaryAddress, m_fineX, m_writeLatch};
}

void PPU::RenderBackground()
{
    BeginFrame();

    const std::uint16_t savedVramAddress = m_vramAddress;
    const BackgroundFetchState savedBackgroundFetch = m_backgroundFetch;
    const std::int16_t savedCycle = m_cycle;
    m_vramAddress = m_temporaryAddress;
    PrimeBackgroundFetch();

    for (std::uint16_t screenY = 0; screenY < 240; ++screenY)
    {
        RenderBackgroundScanline(screenY);
        IncrementY(m_vramAddress);
        CopyHorizontalBits(m_vramAddress, m_temporaryAddress);
        for (int cycle = 321; cycle <= 336; ++cycle)
        {
            m_cycle = cycle;
            ClockBackgroundFetch();
        }
    }

    m_cycle = savedCycle;
    m_vramAddress = savedVramAddress;
    m_backgroundFetch = savedBackgroundFetch;
}

void PPU::BeginFrame()
{
    m_frameBuffer.fill(ColorFromPaletteIndex(PpuRead(0x3F00)));
    m_backgroundOpaque.fill(false);
}

void PPU::RenderBackgroundScanline(std::uint16_t screenY)
{
    for (std::uint16_t screenX = 0; screenX < 256; ++screenX)
    {
        RenderBackgroundPixel(screenY, screenX);
        m_cycle = screenX + 1;
        ClockBackgroundFetch();
    }
}

void PPU::ClockBackgroundFetch()
{
    ShiftBackgroundShifters();

    switch (m_cycle & 0x0007)
    {
    case 1:
        FetchNametableByte();
        break;
    case 3:
        FetchAttribute();
        break;
    case 5:
        FetchPatternLow();
        break;
    case 7:
        FetchPatternHigh();
        break;
    case 0:
        LoadBackgroundShifters();
        IncrementCoarseX(m_vramAddress);
        break;
    default:
        break;
    }
}

void PPU::PrimeBackgroundFetch()
{
    m_backgroundFetch = {};
    for (int tile = 0; tile < 2; ++tile)
    {
        FetchNametableByte();
        FetchAttribute();
        FetchPatternLow();
        FetchPatternHigh();
        LoadBackgroundShifters();
        IncrementCoarseX(m_vramAddress);
        if (tile == 0)
        {
            for (int shift = 0; shift < 8; ++shift)
            {
                ShiftBackgroundShifters();
            }
        }
    }
}

void PPU::FetchNametableByte()
{
    m_backgroundFetch.nametableByte = PpuRead(
        0x2000 | (m_vramAddress & 0x0FFF));
}

void PPU::FetchAttribute()
{
    const std::uint16_t address = 0x23C0 |
        (m_vramAddress & 0x0C00) |
        ((m_vramAddress >> 4) & 0x0038) |
        ((m_vramAddress >> 2) & 0x0007);
    const std::uint8_t shift =
        ((m_vramAddress >> 4) & 0x04) | (m_vramAddress & 0x02);
    m_backgroundFetch.attribute = (PpuRead(address) >> shift) & 0x03;
}

void PPU::FetchPatternLow()
{
    const std::uint16_t patternBase = (m_control & 0x10) ? 0x1000 : 0x0000;
    const std::uint16_t row = (m_vramAddress >> 12) & 0x0007;
    m_backgroundFetch.lowPlane = PpuRead(
        patternBase + m_backgroundFetch.nametableByte * 16 + row);
}

void PPU::FetchPatternHigh()
{
    const std::uint16_t patternBase = (m_control & 0x10) ? 0x1000 : 0x0000;
    const std::uint16_t row = (m_vramAddress >> 12) & 0x0007;
    m_backgroundFetch.highPlane = PpuRead(
        patternBase + m_backgroundFetch.nametableByte * 16 + row + 8);
}

void PPU::LoadBackgroundShifters()
{
    m_backgroundFetch.patternShiftLow =
        (m_backgroundFetch.patternShiftLow & 0xFF00) | m_backgroundFetch.lowPlane;
    m_backgroundFetch.patternShiftHigh =
        (m_backgroundFetch.patternShiftHigh & 0xFF00) | m_backgroundFetch.highPlane;
    m_backgroundFetch.attributeShiftLow =
        (m_backgroundFetch.attributeShiftLow & 0xFF00) |
        ((m_backgroundFetch.attribute & 0x01) != 0 ? 0x00FF : 0x0000);
    m_backgroundFetch.attributeShiftHigh =
        (m_backgroundFetch.attributeShiftHigh & 0xFF00) |
        ((m_backgroundFetch.attribute & 0x02) != 0 ? 0x00FF : 0x0000);
}

void PPU::ShiftBackgroundShifters()
{
    m_backgroundFetch.patternShiftLow <<= 1;
    m_backgroundFetch.patternShiftHigh <<= 1;
    m_backgroundFetch.attributeShiftLow <<= 1;
    m_backgroundFetch.attributeShiftHigh <<= 1;
}

void PPU::RenderBackgroundPixel(std::uint16_t screenY, std::uint16_t screenX)
{
    const std::uint32_t backdrop = ColorFromPaletteIndex(PpuRead(0x3F00));

    if ((m_mask & 0x08) == 0)
    {
        return;
    }

    const std::uint16_t fineXMask = 0x8000 >> m_fineX;
    const std::uint8_t color =
        ((m_backgroundFetch.patternShiftHigh & fineXMask) != 0 ? 0x02 : 0x00) |
        ((m_backgroundFetch.patternShiftLow & fineXMask) != 0 ? 0x01 : 0x00);
    const std::uint8_t palette =
        ((m_backgroundFetch.attributeShiftHigh & fineXMask) != 0 ? 0x02 : 0x00) |
        ((m_backgroundFetch.attributeShiftLow & fineXMask) != 0 ? 0x01 : 0x00);
    const std::uint16_t paletteAddress = color == 0
        ? 0x3F00
        : 0x3F00 + palette * 4 + color;

    const std::size_t pixel = screenY * 256 + screenX;
    const bool visible = screenX >= 8 || (m_mask & 0x02) != 0;
    m_frameBuffer[pixel] = visible
        ? ColorFromPaletteIndex(PpuRead(paletteAddress))
        : backdrop;
    m_backgroundOpaque[pixel] = visible && color != 0;
}

void PPU::RenderSprites()
{
    for (std::uint16_t screenY = 0; screenY < 240; ++screenY)
    {
        EvaluateSpritesForScanline(screenY);
        FetchScanlineSprites(screenY);
        RenderSpritesScanline(screenY);
    }
}

void PPU::RenderSpritesScanline(std::uint16_t screenY)
{
    for (std::uint16_t screenX = 0; screenX < 256; ++screenX)
    {
        RenderSpritePixel(screenY, screenX);
    }
}

void PPU::EvaluateSpritesForScanline(std::uint16_t screenY)
{
    m_scanlineSpriteCount = 0;
    m_sprite0Visible = false;
    if (screenY >= 240)
    {
        return;
    }

    const std::uint16_t spriteHeight = (m_control & 0x20) ? 16 : 8;

    for (std::uint8_t sprite = 0; sprite < 64; ++sprite)
    {
        const std::uint16_t spriteY = m_oam[sprite * 4];
        if (screenY <= spriteY || screenY > spriteY + spriteHeight)
        {
            continue;
        }

        if (m_scanlineSpriteCount == m_scanlineSprites.size())
        {
            m_status |= 0x20;
            break;
        }

        const std::size_t offset = sprite * 4;
        m_scanlineSprites[m_scanlineSpriteCount++] = {
            sprite, m_oam[offset + 3], m_oam[offset + 2], 0, 0};
        if (m_scanlineSpriteCount == 1)
        {
            m_sprite0Visible = sprite == 0;
        }
    }
}

void PPU::FetchScanlineSprites(std::uint16_t screenY)
{
    const std::uint16_t spriteHeight = (m_control & 0x20) ? 16 : 8;
    const std::uint16_t patternBase = (m_control & 0x08) ? 0x1000 : 0x0000;

    for (std::size_t index = 0; index < m_scanlineSpriteCount; ++index)
    {
        ScanlineSprite& scanlineSprite = m_scanlineSprites[index];
        const std::uint8_t sprite = scanlineSprite.index;
        const std::size_t offset = sprite * 4;
        const std::uint16_t spriteY = m_oam[offset];
        std::uint8_t tileIndex = m_oam[offset + 1];
        std::uint16_t patternRow = screenY - spriteY - 1;

        if ((scanlineSprite.attributes & 0x80) != 0)
        {
            patternRow = spriteHeight - 1 - patternRow;
        }

        std::uint16_t tileAddress;
        if (spriteHeight == 16)
        {
            const std::uint16_t spritePatternBase = (tileIndex & 0x01) ? 0x1000 : 0x0000;
            tileIndex &= 0xFE;
            if (patternRow >= 8)
            {
                ++tileIndex;
                patternRow -= 8;
            }
            tileAddress = spritePatternBase + tileIndex * 16 + patternRow;
        }
        else
        {
            tileAddress = patternBase + tileIndex * 16 + patternRow;
        }
        scanlineSprite.lowPlane = PpuRead(tileAddress);
        scanlineSprite.highPlane = PpuRead(tileAddress + 8);
    }
}

void PPU::ProcessSpriteEvaluation()
{
    if (m_cycle < 65)
    {
        // Dots 1-64: the secondary OAM is cleared to $FF, one byte every
        // two dots.
        m_oamCopyBuffer = 0xFF;
        m_secondaryOam[(m_cycle - 1) >> 1] = 0xFF;
        return;
    }
    if (m_cycle > 256)
    {
        return;
    }

    if ((m_cycle & 0x01) != 0)
    {
        // Odd dots read one byte from primary OAM.
        if (m_cycle == 65)
        {
            SpriteEvaluationStart();
        }
        m_oamCopyBuffer = m_oam[m_oamAddress];
        return;
    }

    // Even dots write the latched byte to secondary OAM and advance the
    // evaluation counters. The structure follows the hardware state
    // machine, including the sprite overflow bug: once eight sprites have
    // been found, writes are disabled, the in-range comparison walks the
    // diagonal H/L increments, and the overflow flag latches when the
    // first (mis)read byte lands in range.
    if (m_cycle == 256)
    {
        SpriteEvaluationEnd();
    }

    if (m_oamCopyDone)
    {
        m_spriteAddrH = (m_spriteAddrH + 1) & 0x3F;
        if (m_secondaryOamAddress >= 0x20)
        {
            // With writes disabled, secondary OAM writes become reads.
            m_oamCopyBuffer = m_secondaryOam[m_secondaryOamAddress & 0x1F];
        }
    }
    else
    {
        const std::uint8_t spriteHeight = (m_control & 0x20) ? 16 : 8;
        if (!m_spriteInRange &&
            m_scanline >= m_oamCopyBuffer &&
            m_scanline < m_oamCopyBuffer + spriteHeight)
        {
            m_spriteInRange = !m_oamCopyDone;
        }

        if (m_secondaryOamAddress < 0x20)
        {
            m_secondaryOam[m_secondaryOamAddress] = m_oamCopyBuffer;

            if (m_spriteInRange)
            {
                if (m_cycle == 66)
                {
                    // The very first evaluated byte was in range; hardware
                    // flags this slot as "sprite zero" even when evaluation
                    // started at a nonzero OAMADDR.
                    m_sprite0Added = true;
                }

                ++m_spriteAddrL;
                ++m_secondaryOamAddress;

                if (m_spriteAddrL >= 4)
                {
                    m_spriteAddrH = (m_spriteAddrH + 1) & 0x3F;
                    m_spriteAddrL = 0;
                    if (m_spriteAddrH == 0)
                    {
                        m_oamCopyDone = true;
                    }
                }

                if ((m_secondaryOamAddress & 0x03) == 0)
                {
                    // All four bytes of this sprite were copied. When
                    // evaluation is misaligned (nonzero OAMADDR start), the
                    // byte counter only resynchronizes if the byte just
                    // read is not itself in range.
                    m_spriteInRange = false;
                    if (m_spriteAddrL != 0)
                    {
                        const bool inRange =
                            m_scanline >= m_oamCopyBuffer &&
                            m_scanline < m_oamCopyBuffer + spriteHeight;
                        if (!inRange)
                        {
                            m_spriteAddrL = 0;
                        }
                    }
                }
            }
            else
            {
                // Not in range: skip to the next sprite's Y coordinate.
                m_spriteAddrH = (m_spriteAddrH + 1) & 0x3F;
                m_spriteAddrL = 0;
                if (m_spriteAddrH == 0)
                {
                    m_oamCopyDone = true;
                }
            }
        }
        else
        {
            // Eight sprites have been found: check for overflow and
            // reproduce the hardware bug.
            m_oamCopyBuffer = m_secondaryOam[m_secondaryOamAddress & 0x1F];

            if (m_oamCopyDone)
            {
                m_spriteAddrH = (m_spriteAddrH + 1) & 0x3F;
                m_spriteAddrL = 0;
            }
            else if (m_spriteInRange)
            {
                m_status |= 0x20;
                m_spriteAddrL = m_spriteAddrL + 1;
                if (m_spriteAddrL == 4)
                {
                    m_spriteAddrH = (m_spriteAddrH + 1) & 0x3F;
                    m_spriteAddrL = 0;
                }

                if (m_overflowBugCounter == 0)
                {
                    m_overflowBugCounter = 3;
                }
                else if (m_overflowBugCounter > 0)
                {
                    --m_overflowBugCounter;
                    if (m_overflowBugCounter == 0)
                    {
                        // After "fetching" the overflowed sprite the
                        // evaluator realigns and only dummy reads remain.
                        m_oamCopyDone = true;
                        m_spriteAddrL = 0;
                    }
                }
            }
            else
            {
                // Sprite not on this scanline: the buggy increment steps
                // both counters, scanning OAM diagonally.
                m_spriteAddrH = (m_spriteAddrH + 1) & 0x3F;
                m_spriteAddrL = (m_spriteAddrL + 1) & 0x03;
                if (m_spriteAddrH == 0)
                {
                    m_oamCopyDone = true;
                }
            }
        }
    }

    m_oamAddress = (m_spriteAddrL & 0x03) |
                   (static_cast<std::uint8_t>(m_spriteAddrH) << 2);
}

void PPU::SpriteEvaluationStart()
{
    m_sprite0Added = false;
    m_spriteInRange = false;
    m_secondaryOamAddress = 0;
    m_overflowBugCounter = 0;
    m_oamCopyDone = false;
    // Evaluation interprets whatever OAM byte OAMADDR selects as a sprite
    // Y coordinate.
    m_spriteAddrH = (m_oamAddress >> 2) & 0x3F;
    m_spriteAddrL = m_oamAddress & 0x03;
}

void PPU::SpriteEvaluationEnd()
{
    m_sprite0Visible = m_sprite0Added;
    // Add three to count a partially copied sprite: evaluation that
    // wrapped past OAM can stop mid-sprite.
    m_scanlineSpriteCount = static_cast<std::size_t>(
        (m_secondaryOamAddress + 3) >> 2);
}

void PPU::LoadSpriteTileInfo()
{
    const std::size_t slot = m_spriteFetchIndex;
    const std::uint8_t spriteY = m_secondaryOam[slot * 4];
    const std::uint8_t tileIndex = m_secondaryOam[slot * 4 + 1];
    const std::uint8_t attributes = m_secondaryOam[slot * 4 + 2];
    const std::uint8_t spriteX = m_secondaryOam[slot * 4 + 3];

    const std::uint16_t spriteHeight = (m_control & 0x20) ? 16 : 8;
    bool loaded = false;
    if (slot < m_scanlineSpriteCount && spriteY < 240 && m_scanline >= 0)
    {
        // The slot was fetched during scanline N for display on scanline
        // N+1; the row inside the sprite is N - y.
        std::uint16_t patternRow = m_scanline - spriteY;
        if ((attributes & 0x80) != 0)
        {
            patternRow = spriteHeight - 1 - patternRow;
        }

        std::uint16_t tileAddress;
        if (spriteHeight == 16)
        {
            const std::uint16_t spritePatternBase =
                (tileIndex & 0x01) ? 0x1000 : 0x0000;
            std::uint16_t tile = tileIndex & 0xFE;
            if (patternRow >= 8)
            {
                ++tile;
                patternRow -= 8;
            }
            tileAddress = spritePatternBase + tile * 16 + patternRow;
        }
        else
        {
            const std::uint16_t patternBase =
                (m_control & 0x08) ? 0x1000 : 0x0000;
            tileAddress = patternBase + tileIndex * 16 + patternRow;
        }

        ScanlineSprite& scanlineSprite = m_scanlineSprites[slot];
        scanlineSprite.index = 0;
        scanlineSprite.x = spriteX;
        scanlineSprite.attributes = attributes;
        scanlineSprite.lowPlane = PpuRead(tileAddress);
        scanlineSprite.highPlane = PpuRead(tileAddress + 8);
        loaded = true;
    }

    if (!loaded)
    {
        // Unused slots fetch the transparent tile $FF; the fetches are
        // PPU-bus visible and matter for MMC3-style IRQ counters.
        std::uint16_t garbageAddress;
        if (spriteHeight == 16)
        {
            garbageAddress = 0x1000 + 0xFE * 16;
        }
        else
        {
            const std::uint16_t patternBase =
                (m_control & 0x08) ? 0x1000 : 0x0000;
            garbageAddress = patternBase + 0xFF * 16;
        }
        PpuRead(garbageAddress);
        PpuRead(garbageAddress + 8);
    }

    ++m_spriteFetchIndex;
}

void PPU::RenderSpritePixel(std::uint16_t screenY, std::uint16_t screenX)
{
    if ((m_mask & 0x10) == 0 ||
        (screenX < 8 && (m_mask & 0x04) == 0))
    {
        return;
    }

    // The hardware multiplexer selects the first opaque sprite (lowest
    // slot); only that sprite's pixel competes with the background.
    for (std::size_t slot = 0; slot < m_scanlineSpriteCount; ++slot)
    {
        const ScanlineSprite& sprite = m_scanlineSprites[slot];
        if (screenX < sprite.x || screenX >= sprite.x + 8)
        {
            continue;
        }

        const std::uint8_t column = screenX - sprite.x;
        const std::uint8_t bit =
            (sprite.attributes & 0x40) != 0 ? column : 7 - column;
        const std::uint8_t color =
            ((sprite.highPlane >> bit) & 0x01) << 1 |
            ((sprite.lowPlane >> bit) & 0x01);
        if (color == 0)
        {
            continue;
        }

        const std::size_t pixel = screenY * 256 + screenX;
        if (slot == 0 && m_sprite0Visible &&
            m_backgroundOpaque[pixel] && screenX < 255)
        {
            m_status |= 0x40;
        }
        if ((sprite.attributes & 0x20) != 0 && m_backgroundOpaque[pixel])
        {
            return;
        }

        const std::uint16_t paletteAddress =
            0x3F10 + (sprite.attributes & 0x03) * 4 + color;
        m_frameBuffer[pixel] = ColorFromPaletteIndex(PpuRead(paletteAddress));
        return;
    }
}

bool PPU::PollNmi()
{
    const bool pending = m_nmiPending;
    m_nmiPending = false;
    return pending;
}

bool PPU::ConsumeFrameComplete()
{
    const bool frameComplete = m_frameComplete;
    m_frameComplete = false;
    return frameComplete;
}

std::uint8_t PPU::CpuRead(std::uint16_t address)
{
    std::uint8_t data = m_openBusLatch;
    switch (address & 0x0007)
    {
    case 0x0002:
        // Race window: a read landing one dot before the VBlank flag-set
        // dot returns 0 and suppresses both the flag and the NMI for this
        // frame. The set dot is scanline 241, cycle 1, and CPU accesses
        // fall between PPU dots, so the read sees cycle 1 pending.
        if (m_scanline == 241 && m_cycle == 1)
        {
            m_suppressVblank = true;
        }
        data = (m_status & 0xE0) | (m_openBusLatch & 0x1F);
        m_status &= ~0x80;
        m_nmiPending = false;
        m_writeLatch = false;
        UpdateNmiOutput();
        break;
    case 0x0004:
        data = m_oam[m_oamAddress];
        break;
    case 0x0007:
    {
        const std::uint16_t vramAddress = m_vramAddress;
        const std::uint8_t value = PpuRead(vramAddress);
        IncrementVramAddress();

        if (vramAddress >= 0x3F00)
        {
            m_dataBuffer = PpuRead(vramAddress - 0x1000);
            data = value;
        }
        else
        {
            data = m_dataBuffer;
            m_dataBuffer = value;
        }
        break;
    }
    default:
        // Write-only registers leave the open-bus latch on the CPU bus.
        break;
    }

    m_openBusLatch = data;
    return data;
}

void PPU::CpuWrite(std::uint16_t address, std::uint8_t data)
{
    m_openBusLatch = data;

    switch (address & 0x0007)
    {
    case 0x0000:
        m_control = data;
        m_temporaryAddress = (m_temporaryAddress & 0xF3FF) |
                             ((static_cast<std::uint16_t>(data) & 0x03) << 10);
        UpdateNmiOutput();
        break;
    case 0x0001:
        m_mask = data;
        break;
    case 0x0003:
        m_oamAddress = data;
        break;
    case 0x0004:
        m_oam[m_oamAddress++] = data;
        break;
    case 0x0005:
        if (!m_writeLatch)
        {
            m_fineX = data & 0x07;
            m_temporaryAddress = (m_temporaryAddress & 0xFFE0) | (data >> 3);
        }
        else
        {
            m_temporaryAddress = (m_temporaryAddress & 0x8FFF) |
                                 ((static_cast<std::uint16_t>(data) & 0x07) << 12);
            m_temporaryAddress = (m_temporaryAddress & 0xFC1F) |
                                 ((static_cast<std::uint16_t>(data) & 0xF8) << 2);
        }
        m_writeLatch = !m_writeLatch;
        break;
    case 0x0006:
        if (!m_writeLatch)
        {
            m_temporaryAddress = (m_temporaryAddress & 0x00FF) |
                                 ((static_cast<std::uint16_t>(data) & 0x3F) << 8);
        }
        else
        {
            m_temporaryAddress = (m_temporaryAddress & 0xFF00) | data;
            m_vramAddress = m_temporaryAddress;
        }
        m_writeLatch = !m_writeLatch;
        break;
    case 0x0007:
        PpuWrite(m_vramAddress, data);
        IncrementVramAddress();
        break;
    default:
        break;
    }
}

std::uint8_t PPU::PpuRead(std::uint16_t address)
{
    address = NormalizeAddress(address);
    std::uint8_t data = 0;

    if (address <= 0x1FFF)
    {
        if (m_cartridge && m_cartridge->PpuRead(address, data))
        {
            return data;
        }
        return m_chrRam[address];
    }
    if (address <= 0x3EFF)
    {
        return m_nametableRam[NametableAddress(address)];
    }
    return m_paletteRam[PaletteAddress(address)];
}

void PPU::PpuWrite(std::uint16_t address, std::uint8_t data)
{
    address = NormalizeAddress(address);

    if (address <= 0x1FFF)
    {
        if (m_cartridge)
        {
            m_cartridge->PpuWrite(address, data);
            return;
        }
        m_chrRam[address] = data;
        return;
    }
    if (address <= 0x3EFF)
    {
        m_nametableRam[NametableAddress(address)] = data;
        return;
    }
    m_paletteRam[PaletteAddress(address)] = data;
}

std::uint16_t PPU::NormalizeAddress(std::uint16_t address) const
{
    address &= 0x3FFF;
    return address >= 0x3000 && address <= 0x3EFF ? address - 0x1000 : address;
}

std::uint16_t PPU::NametableAddress(std::uint16_t address) const
{
    const std::uint16_t offset = (address - 0x2000) & 0x0FFF;
    const std::uint16_t table = offset >> 10;
    const std::uint16_t withinTable = offset & 0x03FF;

    // Mappers such as MMC1 switch the arrangement while the PPU runs,
    // so query the cartridge on each nametable access.
    const Mirroring mirroring = m_cartridge != nullptr
        ? m_cartridge->CurrentMirroring()
        : m_mirroring;

    std::uint16_t physicalTable = 0;
    switch (mirroring)
    {
    case Mirroring::Vertical:
        physicalTable = table & 0x0001;
        break;
    case Mirroring::OneScreenLower:
        physicalTable = 0;
        break;
    case Mirroring::OneScreenUpper:
        physicalTable = 1;
        break;
    case Mirroring::Horizontal:
    default:
        physicalTable = table >> 1;
        break;
    }

    return (physicalTable << 10) | withinTable;
}

std::uint8_t PPU::PaletteAddress(std::uint16_t address) const
{
    std::uint8_t paletteAddress = address & 0x001F;
    if ((paletteAddress & 0x13) == 0x10)
    {
        paletteAddress &= 0x0F;
    }
    return paletteAddress;
}

void PPU::IncrementVramAddress()
{
    m_vramAddress += (m_control & 0x04) ? 32 : 1;
    m_vramAddress &= 0x3FFF;
}

void PPU::IncrementCoarseX(std::uint16_t& address) const
{
    if ((address & 0x001F) == 31)
    {
        address &= ~0x001F;
        address ^= 0x0400;
        return;
    }

    ++address;
}

void PPU::IncrementY(std::uint16_t& address) const
{
    if ((address & 0x7000) != 0x7000)
    {
        address += 0x1000;
        return;
    }

    address &= ~0x7000;
    std::uint16_t coarseY = (address & 0x03E0) >> 5;
    if (coarseY == 29)
    {
        coarseY = 0;
        address ^= 0x0800;
    }
    else if (coarseY == 31)
    {
        coarseY = 0;
    }
    else
    {
        ++coarseY;
    }
    address = (address & ~0x03E0) | (coarseY << 5);
}

void PPU::CopyHorizontalBits(std::uint16_t& destination,
                             std::uint16_t source) const
{
    destination = (destination & 0xFBE0) | (source & 0x041F);
}

void PPU::CopyVerticalBits(std::uint16_t& destination,
                           std::uint16_t source) const
{
    destination = (destination & 0x841F) | (source & 0x7BE0);
}

bool PPU::RenderingEnabled() const
{
    return (m_mask & 0x18) != 0;
}

void PPU::UpdateNmiOutput()
{
    const bool output =
        (m_status & 0x80) != 0 && (m_control & 0x80) != 0;
    if (output && !m_nmiOutput)
    {
        m_nmiPending = true;
    }
    m_nmiOutput = output;
}

std::uint32_t PPU::ColorFromPaletteIndex(std::uint8_t index) const
{
    index &= (m_mask & 0x01) ? 0x30 : 0x3F;
    std::uint32_t color = SystemPalette[index];
    std::uint8_t red = color >> 16;
    std::uint8_t green = color >> 8;
    std::uint8_t blue = color;

    if ((m_mask & 0x20) != 0)
    {
        green = static_cast<std::uint8_t>(green * 3 / 4);
        blue = static_cast<std::uint8_t>(blue * 3 / 4);
    }
    if ((m_mask & 0x40) != 0)
    {
        red = static_cast<std::uint8_t>(red * 3 / 4);
        blue = static_cast<std::uint8_t>(blue * 3 / 4);
    }
    if ((m_mask & 0x80) != 0)
    {
        red = static_cast<std::uint8_t>(red * 3 / 4);
        green = static_cast<std::uint8_t>(green * 3 / 4);
    }

    return 0xFF000000 |
           (static_cast<std::uint32_t>(red) << 16) |
           (static_cast<std::uint32_t>(green) << 8) |
           blue;
}

} // namespace dendyforge
