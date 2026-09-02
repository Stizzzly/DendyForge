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

consteval auto MakeOutputPalette()
{
    std::array<std::array<std::uint32_t, 64>, 16> output{};
    for (std::size_t effects = 0; effects < output.size(); ++effects)
    {
        for (std::size_t rawIndex = 0; rawIndex < SystemPalette.size(); ++rawIndex)
        {
            const std::size_t index = rawIndex & ((effects & 0x01) != 0
                ? 0x30
                : 0x3F);
            const std::uint32_t color = SystemPalette[index];
            std::uint8_t red = color >> 16;
            std::uint8_t green = color >> 8;
            std::uint8_t blue = color;

            if ((effects & 0x02) != 0)
            {
                green = static_cast<std::uint8_t>(green * 3 / 4);
                blue = static_cast<std::uint8_t>(blue * 3 / 4);
            }
            if ((effects & 0x04) != 0)
            {
                red = static_cast<std::uint8_t>(red * 3 / 4);
                blue = static_cast<std::uint8_t>(blue * 3 / 4);
            }
            if ((effects & 0x08) != 0)
            {
                red = static_cast<std::uint8_t>(red * 3 / 4);
                green = static_cast<std::uint8_t>(green * 3 / 4);
            }

            output[effects][rawIndex] = 0xFF000000 |
                (static_cast<std::uint32_t>(red) << 16) |
                (static_cast<std::uint32_t>(green) << 8) |
                blue;
        }
    }
    return output;
}

constexpr auto OutputPalette = MakeOutputPalette();

}

// The palette RAM contents an NTSC PPU powers up with (blargg's
// power_up_palette table); games always overwrite it.
constexpr std::array<std::uint8_t, 32> PowerUpPalette{
    0x09, 0x01, 0x00, 0x01, 0x00, 0x02, 0x02, 0x0D,
    0x08, 0x10, 0x08, 0x24, 0x00, 0x00, 0x04, 0x2C,
    0x09, 0x01, 0x34, 0x03, 0x00, 0x04, 0x00, 0x14,
    0x08, 0x3A, 0x00, 0x02, 0x00, 0x20, 0x2C, 0x08,
};

// About half a second of PPU dots: long enough for normal register
// traffic, short enough to decay within blargg's one-second window.
constexpr std::uint32_t OpenBusDecayDots = 89342 * 30;

PPU::PPU()
    : m_paletteRam(PowerUpPalette),
      m_outputPalette(OutputPalette[0])
{
    // Secondary OAM is observed as empty until the first visible-line
    // evaluation has populated it. This also keeps the pre-render fetches
    // from treating zero-initialized host memory as eight live sprites.
    m_secondaryOam.fill(0xFF);
}

void PPU::ConnectCartridge(Cartridge* cartridge)
{
    m_cartridge = cartridge;
    m_mapperMonitorsPpuBus = cartridge && cartridge->MonitorsPpuBus();
    m_mirroring = cartridge ? cartridge->Info().MirroringMode()
                            : Mirroring::Horizontal;
}

void PPU::Clock()
{
    ClockPendingRenderingMaskUpdate();
    const bool ppuDataBusCollisionActive =
        m_ppuDataBusCollisionPending && m_ppuDataBusCollisionDelay == 0;

    const bool renderingScanline = m_scanline >= -1 && m_scanline < 240;
    if (m_oamCorruptionPending && renderingScanline && RenderingEnabled())
    {
        ApplyPendingOamCorruption();
    }

    if (m_mapperMonitorsPpuBus)
    {
        m_cartridge->PpuClock();
    }

    if (m_openBusLatch != 0 && ++m_openBusDecayDots >= OpenBusDecayDots)
    {
        m_openBusLatch = 0;
    }

    if (m_scanline == -1 && m_cycle == 1)
    {
        m_status &= ~0xE0;
        BeginFrame();
    }

    const bool backgroundFetchCycle =
        (m_cycle >= 1 && m_cycle <= 256) ||
        (m_cycle >= 321 && m_cycle <= 336);

    if (m_scanline >= 0 && m_scanline < 240 &&
        m_cycle >= 1 && m_cycle <= 256)
    {
        RenderBackgroundPixel(m_scanline, m_cycle - 1);
        RenderSpritePixel(m_scanline, m_cycle - 1);
        ClockSpriteUnits();
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
        if (m_scanline == -1 && m_cycle >= 280 && m_cycle <= 304)
        {
            CopyVerticalBits(m_vramAddress, m_temporaryAddress);
        }

        if (m_cycle >= 257 && m_cycle <= 320)
        {
            // Sprite tile loading interval: OAMADDR is held at zero, and
            // each of the eight slots consumes eight dots (two garbage
            // nametable fetches, then the two pattern bytes at
            // (cycle - 257) & 7 == 4).
            m_oamAddress = 0;
            const std::uint8_t fetchDot =
                static_cast<std::uint8_t>(m_cycle - 257);
            const std::uint8_t slot = fetchDot >> 3;
            const std::uint8_t phase = fetchDot & 0x07;
            const std::uint8_t byte = phase < 3 ? phase : 3;
            m_oamCopyBuffer = m_secondaryOam[slot * 4 + byte];
            m_oamCopyBufferIsAttribute = byte == 2;
            if (m_cycle == 257)
            {
                if (m_scanline == -1)
                {
                    // The pre-render line does not run normal evaluation,
                    // but its fetch interval reuses secondary OAM and tests
                    // sprite range as scanline 261 & $FF == 5. This can put
                    // preserved sprite data into the shifters for scanline 0.
                    m_scanlineSpriteCount = 8;
                    m_sprite0Visible = false;
                }
            }
            switch ((m_cycle - 257) & 0x07)
            {
            case 0:
                ReadRenderingBus(0x2000 | (m_vramAddress & 0x0FFF));
                break;
            case 2:
                ReadRenderingBus(0x2000 | (m_vramAddress & 0x0FFF));
                break;
            case 4:
                FetchSpritePatternLow();
                break;
            case 6:
                FetchSpritePatternHigh();
                break;
            default:
                break;
            }
        }

        if (m_cycle == 257)
        {
            // The first garbage nametable fetch above observes the old v;
            // the horizontal scroll bits are reloaded from t afterwards.
            CopyHorizontalBits(m_vramAddress, m_temporaryAddress);
        }

        if (m_cycle >= 321 && m_cycle <= 340)
        {
            // The secondary-OAM address wraps after the eighth fetch slot;
            // byte zero remains on the internal OAM bus through HBlank.
            m_oamCopyBuffer = m_secondaryOam[0];
            m_oamCopyBufferIsAttribute = false;
        }

        if (m_cycle == 337 || m_cycle == 339)
        {
            // The two dummy nametable fetches that end the scanline.
            ReadRenderingBus(0x2000 | (m_vramAddress & 0x0FFF));
        }
        if (m_cycle == 339)
        {
            // Fetching loads each X counter in a halted state. Dot 339 is
            // the separate enable pulse that starts nonzero counters; if
            // forced blank covers this dot they remain halted and output
            // their stale shifters as soon as rendering resumes.
            for (std::size_t slot = 0; slot < m_scanlineSpriteCount; ++slot)
            {
                auto& sprite = m_scanlineSprites[slot];
                sprite.counterCounting = sprite.xCounter != 0;
            }
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
        }
        m_frameComplete = true;
    }

    ClockPendingVramAddressUpdate();
    if (ppuDataBusCollisionActive)
    {
        // A collision belongs to one physical dot. ReadRenderingBus may
        // already have consumed it; otherwise the dot had no collapsed
        // external read and the feedback pulse simply expires.
        m_ppuDataBusCollisionPending = false;
    }
    ClockPendingPpuDataRead();

    const bool skipOddFrameDot =
        m_scanline == -1 && m_cycle == 339 && m_oddFrame &&
        RenderingEnabled();

    ++m_cycle;
    if (skipOddFrameDot && m_scanline == -1 && m_cycle == 340)
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
    const std::uint8_t savedEffectiveRenderingMask = m_effectiveRenderingMask;
    const std::uint8_t savedPendingRenderingMask = m_pendingRenderingMask;
    const std::uint8_t savedRenderingMaskUpdateDelay = m_renderingMaskUpdateDelay;
    m_effectiveRenderingMask = m_mask & 0x18;
    m_renderingMaskUpdateDelay = 0;

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
    m_effectiveRenderingMask = savedEffectiveRenderingMask;
    m_pendingRenderingMask = savedPendingRenderingMask;
    m_renderingMaskUpdateDelay = savedRenderingMaskUpdateDelay;
}

void PPU::BeginFrame()
{
    m_frameBuffer.fill(ColorFromPaletteIndex(m_paletteRam[0]));
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
    m_backgroundFetch.nametableByte = ReadRenderingBus(
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
    m_backgroundFetch.attribute = (ReadRenderingBus(address) >> shift) & 0x03;
}

void PPU::FetchPatternLow()
{
    const std::uint16_t patternBase = (m_control & 0x10) ? 0x1000 : 0x0000;
    const std::uint16_t row = (m_vramAddress >> 12) & 0x0007;
    m_backgroundFetch.lowPlane = ReadRenderingBus(
        patternBase + m_backgroundFetch.nametableByte * 16 + row);
}

void PPU::FetchPatternHigh()
{
    const std::uint16_t patternBase = (m_control & 0x10) ? 0x1000 : 0x0000;
    const std::uint16_t row = (m_vramAddress >> 12) & 0x0007;
    m_backgroundFetch.highPlane = ReadRenderingBus(
        patternBase + m_backgroundFetch.nametableByte * 16 + row + 8);
}

std::uint8_t PPU::ReadRenderingBus(std::uint16_t address)
{
    if (m_ppuDataBusCollisionPending &&
        m_ppuDataBusCollisionDelay == 0)
    {
        // ALE and /RD can overlap because the lower eight external pins are
        // multiplexed between address and data. In the stable collision
        // case, the previous data byte feeds back into the octal address
        // latch while the upper address bits still come from the PAR.
        address = static_cast<std::uint16_t>(
            (address & 0x3F00) | m_renderingReadBus);
        m_ppuDataBusCollisionPending = false;
    }
    m_renderingReadBus = PpuRead(address);
    return m_renderingReadBus;
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
    // The RP2C02's two pattern shifters have opposite serial inputs:
    // plane 0 shifts in zero, while plane 1 shifts in one. Normally the
    // freshly fetched tile replaces these bits every eight dots, but the
    // distinction is observable when rendering is toggled around the load.
    m_backgroundFetch.patternShiftHigh =
        static_cast<std::uint16_t>((m_backgroundFetch.patternShiftHigh << 1) | 1);
    m_backgroundFetch.attributeShiftLow <<= 1;
    m_backgroundFetch.attributeShiftHigh <<= 1;
}

void PPU::RenderBackgroundPixel(std::uint16_t screenY, std::uint16_t screenX)
{
    if ((m_effectiveRenderingMask & 0x08) == 0)
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
        ? ColorFromPaletteIndex(m_paletteRam[PaletteAddress(paletteAddress)])
        : ColorFromPaletteIndex(m_paletteRam[0]);
    m_backgroundOpaque[pixel] = visible && color != 0;
}

void PPU::RenderSprites()
{
    const std::uint8_t savedEffectiveRenderingMask = m_effectiveRenderingMask;
    m_effectiveRenderingMask = m_mask & 0x18;
    m_bulkSpriteRendering = true;
    for (std::uint16_t screenY = 0; screenY < 240; ++screenY)
    {
        EvaluateSpritesForScanline(screenY);
        FetchScanlineSprites(screenY);
        RenderSpritesScanline(screenY);
    }
    m_bulkSpriteRendering = false;
    m_effectiveRenderingMask = savedEffectiveRenderingMask;
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
        m_oamCopyBufferIsAttribute = false;
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
        m_oamCopyBufferIsAttribute = (m_oamAddress & 0x03) == 0x02;
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
        // With a full secondary OAM, the even phase reads its wrapped byte
        // zero. If primary OAM wrapped first, the internal bus instead keeps
        // the terminal primary byte that ended evaluation.
        if (m_secondaryOamAddress >= 0x20)
        {
            const std::uint8_t secondaryAddress =
                m_secondaryOamAddress & 0x1F;
            m_oamCopyBuffer = m_secondaryOam[secondaryAddress];
            m_oamCopyBufferIsAttribute =
                (secondaryAddress & 0x03) == 0x02;
        }
        else
        {
            m_oamCopyBuffer = m_oamCopyDoneBuffer;
            m_oamCopyBufferIsAttribute =
                m_oamCopyDoneBufferIsAttribute;
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
                        m_oamCopyDoneBuffer = m_oamCopyBuffer;
                        m_oamCopyDoneBufferIsAttribute =
                            m_oamCopyBufferIsAttribute;
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
                    m_oamCopyDoneBuffer = m_oamCopyBuffer;
                    m_oamCopyDoneBufferIsAttribute =
                        m_oamCopyBufferIsAttribute;
                }
            }
        }
        else
        {
            // Eight sprites have been found: check for overflow and
            // reproduce the hardware bug.
            const std::uint8_t secondaryAddress =
                m_secondaryOamAddress & 0x1F;
            m_oamCopyBuffer = m_secondaryOam[secondaryAddress];
            m_oamCopyBufferIsAttribute =
                (secondaryAddress & 0x03) == 0x02;

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
                else if (--m_overflowBugCounter == 0)
                {
                    // Once the extra sprite's remaining bytes have been
                    // consumed, the low counter realigns and evaluation
                    // continues as failed copies from successive sprites.
                    m_oamCopyDone = true;
                    m_oamCopyDoneBuffer = m_oamCopyBuffer;
                    m_oamCopyDoneBufferIsAttribute =
                        m_oamCopyBufferIsAttribute;
                    m_spriteAddrL = 0;
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
                    m_oamCopyDoneBuffer = m_oamCopyBuffer;
                    m_oamCopyDoneBufferIsAttribute =
                        m_oamCopyBufferIsAttribute;
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

void PPU::FetchSpritePatternLow()
{
    // Derive the slot from the physical fetch phase. Rendering can be
    // disabled at dot 257 and re-enabled part-way through dots 257-320;
    // retaining the previous scanline's software counter in that case can
    // address a ninth secondary-OAM entry even though hardware always has
    // exactly eight fetch slots.
    const std::size_t slot = static_cast<std::size_t>(
        (m_cycle - 257) >> 3);
    const std::uint8_t spriteY = m_secondaryOam[slot * 4];
    const std::uint8_t tileIndex = m_secondaryOam[slot * 4 + 1];
    const std::uint8_t attributes = m_secondaryOam[slot * 4 + 2];
    const std::uint8_t spriteX = m_secondaryOam[slot * 4 + 3];
    ScanlineSprite& scanlineSprite = m_scanlineSprites[slot];
    scanlineSprite.x = spriteX;
    scanlineSprite.xCounter = spriteX;
    scanlineSprite.counterCounting = false;

    const std::uint16_t spriteHeight = (m_control & 0x20) ? 16 : 8;
    const std::int16_t evaluationScanline =
        m_scanline == -1 ? 5 : m_scanline;
    if (slot < m_scanlineSpriteCount && spriteY < 240 &&
        evaluationScanline >= spriteY &&
        evaluationScanline < spriteY + spriteHeight)
    {
        // The slot was fetched during scanline N for display on scanline
        // N+1; the row inside the sprite is N - y.
        std::uint16_t patternRow =
            static_cast<std::uint16_t>(evaluationScanline - spriteY);
        if ((attributes & 0x80) != 0)
        {
            patternRow = spriteHeight - 1 - patternRow;
        }

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
            m_spritePatternAddress =
                spritePatternBase + tile * 16 + patternRow;
        }
        else
        {
            const std::uint16_t patternBase =
                (m_control & 0x08) ? 0x1000 : 0x0000;
            m_spritePatternAddress = patternBase + tileIndex * 16 + patternRow;
        }

        m_scanlineSpritePatternValid[slot] = true;
        scanlineSprite.index = 0;
        scanlineSprite.x = spriteX;
        scanlineSprite.attributes = attributes;
        scanlineSprite.lowPlane = ReadRenderingBus(m_spritePatternAddress);
        if (m_scanline == -1 && slot == 0)
        {
            m_sprite0Visible = true;
        }
        return;
    }

    // Sprite evaluation may have selected this secondary-OAM entry while
    // PPUCTRL still described a 16-pixel sprite. If software changes to
    // 8-pixel sprites before the pattern fetch, the slot remains allocated
    // but its output shifters are loaded transparent. Do not leave either
    // plane from the preceding scanline in the software representation.
    m_scanlineSpritePatternValid[slot] = false;
    m_scanlineSprites[slot].lowPlane = 0;
    m_scanlineSprites[slot].highPlane = 0;

    // Unused slots fetch the transparent tile $FF. Its two planes still
    // occupy distinct PPU fetch phases and remain mapper-bus visible.
    if (spriteHeight == 16)
    {
        m_spritePatternAddress = 0x1000 + 0xFE * 16;
    }
    else
    {
        const std::uint16_t patternBase =
            (m_control & 0x08) ? 0x1000 : 0x0000;
        m_spritePatternAddress = patternBase + 0xFF * 16;
    }
    ReadRenderingBus(m_spritePatternAddress);
}

void PPU::FetchSpritePatternHigh()
{
    const std::size_t slot = static_cast<std::size_t>(
        (m_cycle - 257) >> 3);
    if (slot < m_scanlineSpriteCount &&
        m_scanlineSpritePatternValid[slot])
    {
        m_scanlineSprites[slot].highPlane =
            ReadRenderingBus(m_spritePatternAddress + 8);
    }
    else
    {
        ReadRenderingBus(m_spritePatternAddress + 8);
    }

}

void PPU::RenderSpritePixel(std::uint16_t screenY, std::uint16_t screenX)
{
    if ((m_effectiveRenderingMask & 0x10) == 0 ||
        (screenX < 8 && (m_mask & 0x04) == 0))
    {
        return;
    }

    // The hardware multiplexer selects the first opaque sprite (lowest
    // slot); only that sprite's pixel competes with the background.
    for (std::size_t slot = 0; slot < m_scanlineSpriteCount; ++slot)
    {
        const ScanlineSprite& sprite = m_scanlineSprites[slot];
        std::uint8_t bit = 0;
        if (m_bulkSpriteRendering)
        {
            if (screenX < sprite.x || screenX >= sprite.x + 8)
            {
                continue;
            }
            const std::uint8_t column = screenX - sprite.x;
            bit = (sprite.attributes & 0x40) != 0 ? column : 7 - column;
        }
        else
        {
            if (sprite.counterCounting)
            {
                continue;
            }
            bit = (sprite.attributes & 0x40) != 0 ? 0 : 7;
        }

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
        m_frameBuffer[pixel] = ColorFromPaletteIndex(
            m_paletteRam[PaletteAddress(paletteAddress)]);
        return;
    }
}

bool PPU::NmiLineLevel() const
{
    return (m_status & 0x80) != 0 && (m_control & 0x80) != 0;
}

bool PPU::ConsumeFrameComplete()
{
    const bool frameComplete = m_frameComplete;
    m_frameComplete = false;
    return frameComplete;
}

std::optional<std::uint8_t> PPU::DebugPeekCpuRegister(std::uint16_t address) const
{
    switch (address & 0x0007)
    {
    case 0x0000: return m_control;
    case 0x0001: return m_mask;
    case 0x0002: return m_status;
    case 0x0003: return m_oamAddress;
    case 0x0004: return m_oam[m_oamAddress];
    case 0x0007: return m_dataBuffer;
    default: return std::nullopt;
    }
}

void PPU::ClockSpriteUnits()
{
    for (std::size_t slot = 0; slot < m_scanlineSpriteCount; ++slot)
    {
        ScanlineSprite& sprite = m_scanlineSprites[slot];
        if (sprite.counterCounting)
        {
            if (sprite.xCounter > 0 && --sprite.xCounter == 0)
            {
                sprite.counterCounting = false;
            }
            continue;
        }

        // X counters keep running through forced blank, but the pattern
        // shifters themselves are gated by either rendering enable.
        if (RenderingEnabled())
        {
            if ((sprite.attributes & 0x40) != 0)
            {
                sprite.lowPlane >>= 1;
                sprite.highPlane >>= 1;
            }
            else
            {
                sprite.lowPlane <<= 1;
                sprite.highPlane <<= 1;
            }
        }
    }
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
        // The VBlank flag is latched when the CPU read begins, but the two
        // sprite flags remain connected until M2 falls. In this scheduler a
        // read beginning with pre-render dot 1 pending spans the physical
        // dot-1 clear, so it returns old VBlank with cleared sprite flags.
        // Keep the physical flags intact until Clock() executes that dot.
        data = (m_status & 0x80) |
               ((m_scanline == -1 && m_cycle == 1) ? 0 : (m_status & 0x60)) |
               (m_openBusLatch & 0x1F);
        m_status &= ~0x80;
        m_writeLatch = false;
        m_openBusLatch =
            (m_openBusLatch & 0x1F) | (data & 0xE0);
        return data;
    case 0x0004:
        if (m_scanline >= 0 && m_scanline < 240 && RenderingEnabled())
        {
            // During rendering $2004 exposes the internal evaluation/fetch
            // bus. Attribute bits were already absent when primary OAM was
            // written; do not mask a secondary-OAM byte merely because the
            // current fetch phase happens to address an attribute slot.
            // Empty secondary OAM must therefore remain $FF, not $E3.
            data = m_oamCopyBuffer;
        }
        else
        {
            data = m_oam[m_oamAddress];
            // Bits 2-4 of an attribute byte do not exist on the chip.
            if ((m_oamAddress & 0x03) == 0x02)
            {
                data &= 0xE3;
            }
        }
        break;
    case 0x0007:
    {
        const std::uint16_t vramAddress = m_vramAddress;
        const bool renderingAccess =
            RenderingEnabled() && m_scanline >= -1 && m_scanline < 240;
        const std::uint8_t value = renderingAccess && vramAddress < 0x3F00
            ? 0
            : PpuRead(vramAddress);
        if (renderingAccess)
        {
            m_ppuDataIncrementPending = true;
            m_ppuDataIncrementDelay = 4;
        }
        else
        {
            IncrementVramAddress();
        }

        if (vramAddress >= 0x3F00)
        {
            m_dataBuffer = PpuRead(vramAddress - 0x1000);
            // Palette RAM is six bits wide: the top two bits keep their
            // decayed open-bus value (blargg ppu_open_bus).
            const std::uint8_t paletteMask =
                (m_mask & 0x01) != 0 ? 0x30 : 0x3F;
            data = (value & paletteMask) | (m_openBusLatch & 0xC0);
            m_openBusLatch = data;
            return data;
        }
        else
        {
            data = m_dataBuffer;
            if (renderingAccess)
            {
                // $2007 starts its own external-read state machine. The
                // rendering cadence already owns the multiplexed address /
                // data pins. CpuRead() is invoked at the beginning of the
                // CPU bus cycle; M2 falls after the console has clocked its
                // three PPU dots. The read-state machine then reaches its
                // first stable cadence sample on the following PPU dot in
                // this whole-dot model, rather than reading v's logical
                // address immediately.
                m_ppuDataReadPending = true;
                m_ppuDataReadDelay = 4;
                m_ppuDataBusCollisionPending = true;
                m_ppuDataBusCollisionDelay = 5;
            }
            else
            {
                m_dataBuffer = value;
            }
        }
        break;
    }
    default:
        // Write-only registers drive nothing onto the CPU bus: the
        // decayed latch value comes back unchanged and the decay timer
        // keeps running (blargg ppu_open_bus).
        return m_openBusLatch;
    }

    m_openBusLatch = data;
    m_openBusDecayDots = 0;
    return data;
}

void PPU::CpuWrite(std::uint16_t address, std::uint8_t data)
{
    m_openBusLatch = data;
    m_openBusDecayDots = 0;

    switch (address & 0x0007)
    {
    case 0x0000:
        m_control = data;
        m_temporaryAddress = (m_temporaryAddress & 0xF3FF) |
                             ((static_cast<std::uint16_t>(data) & 0x03) << 10);
        break;
    case 0x0001:
        m_mask = data;
        m_outputPalette = OutputPalette[
            (data & 0x01) | ((data >> 4) & 0x0E)];
        m_pendingRenderingMask = data & 0x18;
        // PPUMASK's rendering-enable signals pass through several PPU
        // latches. With the console's fixed CPU/PPU phase, four upcoming
        // dot clocks reproduce the observed roughly 3-4 dot latency.
        m_renderingMaskUpdateDelay = 4;
        break;
    case 0x0003:
        m_oamAddress = data;
        break;
    case 0x0004:
        if (m_scanline >= -1 && m_scanline < 240 && RenderingEnabled())
        {
            // During rendering OAM writes are blocked. The internal address
            // still advances to the next row and loses its low two bits.
            m_oamAddress = static_cast<std::uint8_t>(
                (m_oamAddress + 4) & 0xFC);
        }
        else
        {
            if ((m_oamAddress & 0x03) == 0x02)
            {
                // Attribute bits 2-4 have no storage cells. Mask on the
                // physical write so sprite evaluation sees the same value
                // that a later CPU read exposes.
                data &= 0xE3;
            }
            m_oam[m_oamAddress++] = data;
        }
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
            if (RenderingEnabled())
            {
                // During rendering, the PPU applies a completed $2006
                // address write after three PPU dots. MMC3 IRQ handlers
                // rely on this latency for split-scroll changes.
                m_pendingVramAddress = m_temporaryAddress;
                m_vramAddressUpdateDelay = 3;
            }
            else
            {
                m_vramAddress = m_temporaryAddress;
            }
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

    // Palette RAM is inside the PPU. Its logical reads (including the
    // lookup performed for every rendered pixel) do not drive the
    // cartridge-visible PPU address bus, so they must not create MMC3
    // A12 edges. Pattern-table and nametable fetches do drive it.
    if (m_mapperMonitorsPpuBus && address <= 0x3EFF)
    {
        m_cartridge->ObservePpuAddress(address);
    }

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

    // See PpuRead: palette RAM is internal to the PPU and invisible to
    // mappers monitoring PPU A12.
    if (m_mapperMonitorsPpuBus && address <= 0x3EFF)
    {
        m_cartridge->ObservePpuAddress(address);
    }

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
    if (RenderingEnabled() && m_scanline >= -1 && m_scanline < 240)
    {
        // During an active rendering scanline the ordinary +1/+32 input is
        // disconnected. A CPU access to $2007 clocks both scroll counters,
        // exactly like the dot-256 vertical increment and a tile-boundary
        // horizontal increment occurring together (usually v += $1001).
        IncrementCoarseX(m_vramAddress);
        IncrementY(m_vramAddress);
        return;
    }

    m_vramAddress += (m_control & 0x04) ? 32 : 1;
    m_vramAddress &= 0x3FFF;
}

void PPU::ClockPendingVramAddressUpdate()
{
    if (m_vramAddressUpdateDelay == 0)
    {
        return;
    }

    --m_vramAddressUpdateDelay;
    if (m_vramAddressUpdateDelay == 0)
    {
        m_vramAddress = m_pendingVramAddress;
    }
}

void PPU::ClockPendingPpuDataRead()
{
    if (m_ppuDataReadPending && m_ppuDataReadDelay != 0 &&
        --m_ppuDataReadDelay == 0)
    {
        m_dataBuffer = m_renderingReadBus;
        m_ppuDataReadPending = false;
    }

    if (m_ppuDataIncrementPending && m_ppuDataIncrementDelay != 0 &&
        --m_ppuDataIncrementDelay == 0)
    {
        IncrementVramAddress();
        m_ppuDataIncrementPending = false;
    }

    if (m_ppuDataBusCollisionPending &&
        m_ppuDataBusCollisionDelay != 0)
    {
        --m_ppuDataBusCollisionDelay;
    }
}

void PPU::ClockPendingRenderingMaskUpdate()
{
    if (m_renderingMaskUpdateDelay == 0)
    {
        return;
    }

    if (--m_renderingMaskUpdateDelay == 0)
    {
        const bool wasRendering = RenderingEnabled();
        m_effectiveRenderingMask = m_pendingRenderingMask;
        if (wasRendering && !RenderingEnabled() &&
            m_scanline >= -1 && m_scanline < 240)
        {
            // Turning both rendering pipelines off during sprite activity
            // preserves the current secondary-OAM address as a corruption
            // seed. Primary OAM is not changed until rendering is enabled
            // again on a pre-render or visible scanline.
            m_oamCorruptionPending = true;
            m_oamCorruptionRow = OamCorruptionRow();
        }
    }
}

std::uint8_t PPU::OamCorruptionRow() const
{
    if (m_cycle >= 1 && m_cycle <= 64)
    {
        // Secondary OAM advances once per clear write. Which side of the
        // write a CPU-visible transition lands on depends on the CPU/PPU
        // phase; the console's fixed phase observes the post-write value.
        return static_cast<std::uint8_t>((m_cycle >> 1) & 0x1F);
    }

    if (m_cycle >= 65 && m_cycle <= 256)
    {
        // During evaluation a partial sprite is completed to the next
        // four-byte secondary-OAM boundary before its row is selected.
        return static_cast<std::uint8_t>(
            (m_secondaryOamAddress + 3) & 0x1C);
    }

    if (m_cycle >= 257 && m_cycle <= 320)
    {
        const std::uint8_t fetchDot =
            static_cast<std::uint8_t>(m_cycle - 257);
        const std::uint8_t slotBase =
            static_cast<std::uint8_t>((fetchDot >> 3) * 4);
        const std::uint8_t phase = fetchDot & 0x07;
        const std::uint8_t withinSlot = phase <= 2 ? phase + 1
                                           : phase == 7 ? 4
                                                        : 3;
        return static_cast<std::uint8_t>((slotBase + withinSlot) & 0x1F);
    }

    return 0;
}

void PPU::ApplyPendingOamCorruption()
{
    const std::size_t primaryOffset =
        static_cast<std::size_t>(m_oamCorruptionRow) * 8;
    for (std::size_t byte = 0; byte < 8; ++byte)
    {
        m_oam[primaryOffset + byte] = m_oam[byte];
    }
    m_secondaryOam[m_oamCorruptionRow & 0x1F] = m_secondaryOam[0];
    m_oamCorruptionPending = false;
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
    return m_effectiveRenderingMask != 0;
}


std::uint32_t PPU::ColorFromPaletteIndex(std::uint8_t index) const
{
    return m_outputPalette[index & 0x3F];
}

} // namespace dendyforge
