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
    }
    else if (m_scanline == 241 && m_cycle == 1)
    {
        RenderBackground();
        RenderSprites();
        m_status |= 0x80;
        m_nmiPending = (m_control & 0x80) != 0;
    }

    ++m_cycle;
    if (m_cycle < 341)
    {
        return;
    }

    m_cycle = 0;
    ++m_scanline;
    if (m_scanline == 261)
    {
        m_scanline = -1;
    }
}

const std::array<std::uint32_t, 256 * 240>& PPU::FrameBuffer() const
{
    return m_frameBuffer;
}

void PPU::RenderBackground()
{
    const std::uint32_t backdrop = ColorFromPaletteIndex(PpuRead(0x3F00));
    m_frameBuffer.fill(backdrop);
    m_backgroundOpaque.fill(false);

    if ((m_mask & 0x08) == 0)
    {
        return;
    }

    const std::uint16_t patternBase = (m_control & 0x10) ? 0x1000 : 0x0000;
    const std::uint8_t baseNametableX = m_control & 0x01;
    const std::uint8_t baseNametableY = (m_control >> 1) & 0x01;

    for (std::uint16_t y = 0; y < 240; ++y)
    {
        const std::uint16_t worldY = y + m_scrollY;
        const std::uint16_t tileRow = (worldY / 8) % 30;
        const std::uint16_t rowInTile = worldY & 0x07;
        const std::uint8_t nametableY =
            baseNametableY ^ ((worldY / 240) & 0x01);

        for (std::uint16_t x = 0; x < 256; ++x)
        {
            const std::uint16_t worldX = x + m_scrollX;
            const std::uint16_t tileColumn = (worldX / 8) & 0x1F;
            const std::uint8_t nametableX =
                baseNametableX ^ ((worldX / 256) & 0x01);
            const std::uint16_t nametableBase =
                0x2000 | ((nametableY << 1 | nametableX) << 10);
            const std::uint8_t tileIndex = PpuRead(
                nametableBase + tileRow * 32 + tileColumn);
            const std::uint8_t attribute = PpuRead(
                nametableBase + 0x03C0 + (tileRow / 4) * 8 + (tileColumn / 4));
            const std::uint8_t attributeShift =
                ((tileRow & 0x02) ? 4 : 0) | ((tileColumn & 0x02) ? 2 : 0);
            const std::uint8_t palette = (attribute >> attributeShift) & 0x03;

            const std::uint16_t tileAddress = patternBase + tileIndex * 16 + rowInTile;
            const std::uint8_t lowPlane = PpuRead(tileAddress);
            const std::uint8_t highPlane = PpuRead(tileAddress + 8);
            const std::uint8_t bit = 7 - (x & 0x07);
            const std::uint8_t color =
                ((highPlane >> bit) & 0x01) << 1 | ((lowPlane >> bit) & 0x01);
            const std::uint16_t paletteAddress = color == 0
                ? 0x3F00
                : 0x3F00 + palette * 4 + color;

            const std::size_t pixel = y * 256 + x;
            m_frameBuffer[pixel] = ColorFromPaletteIndex(PpuRead(paletteAddress));
            m_backgroundOpaque[pixel] = color != 0;
        }
    }
}

void PPU::RenderSprites()
{
    if ((m_mask & 0x10) == 0)
    {
        return;
    }

    const std::uint16_t spriteHeight = (m_control & 0x20) ? 16 : 8;
    const std::uint16_t patternBase = (m_control & 0x08) ? 0x1000 : 0x0000;

    for (std::uint16_t screenY = 0; screenY < 240; ++screenY)
    {
        std::array<std::uint8_t, 8> visibleSprites{};
        std::size_t visibleCount = 0;

        for (std::uint8_t sprite = 0; sprite < 64; ++sprite)
        {
            const std::uint16_t spriteY = m_oam[sprite * 4];
            if (screenY <= spriteY || screenY > spriteY + spriteHeight)
            {
                continue;
            }

            if (visibleCount == visibleSprites.size())
            {
                m_status |= 0x20;
                break;
            }

            visibleSprites[visibleCount++] = sprite;
        }

        for (std::size_t index = visibleCount; index > 0; --index)
        {
            const std::uint8_t sprite = visibleSprites[index - 1];
            const std::size_t offset = sprite * 4;
            const std::uint16_t spriteY = m_oam[offset];
            std::uint8_t tileIndex = m_oam[offset + 1];
            const std::uint8_t attributes = m_oam[offset + 2];
            const std::uint8_t spriteX = m_oam[offset + 3];
            std::uint16_t patternRow = screenY - spriteY - 1;

            if ((attributes & 0x80) != 0)
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
            const std::uint8_t lowPlane = PpuRead(tileAddress);
            const std::uint8_t highPlane = PpuRead(tileAddress + 8);

            for (std::uint16_t column = 0; column < 8; ++column)
            {
                const std::uint16_t screenX = spriteX + column;
                if (screenX >= 256)
                {
                    continue;
                }

                const std::uint8_t bit = (attributes & 0x40) ? column : 7 - column;
                const std::uint8_t color =
                    ((highPlane >> bit) & 0x01) << 1 | ((lowPlane >> bit) & 0x01);
                if (color == 0)
                {
                    continue;
                }

                const std::size_t pixel = screenY * 256 + screenX;
                if (sprite == 0 && m_backgroundOpaque[pixel] && screenX < 255)
                {
                    m_status |= 0x40;
                }
                if ((attributes & 0x20) != 0 && m_backgroundOpaque[pixel])
                {
                    continue;
                }

                const std::uint16_t paletteAddress =
                    0x3F10 + (attributes & 0x03) * 4 + color;
                m_frameBuffer[pixel] = ColorFromPaletteIndex(PpuRead(paletteAddress));
            }
        }
    }
}

bool PPU::PollNmi()
{
    const bool pending = m_nmiPending;
    m_nmiPending = false;
    return pending;
}

std::uint8_t PPU::CpuRead(std::uint16_t address)
{
    switch (address & 0x0007)
    {
    case 0x0002:
    {
        const std::uint8_t data = (m_status & 0xE0) | (m_dataBuffer & 0x1F);
        m_status &= ~0x80;
        m_nmiPending = false;
        m_writeLatch = false;
        return data;
    }
    case 0x0004:
        return m_oam[m_oamAddress];
    case 0x0007:
    {
        const std::uint16_t address = m_vramAddress;
        const std::uint8_t value = PpuRead(address);
        IncrementVramAddress();

        if (address >= 0x3F00)
        {
            m_dataBuffer = PpuRead(address - 0x1000);
            return value;
        }

        const std::uint8_t data = m_dataBuffer;
        m_dataBuffer = value;
        return data;
    }
    default:
        return 0;
    }
}

void PPU::CpuWrite(std::uint16_t address, std::uint8_t data)
{
    switch (address & 0x0007)
    {
    case 0x0000:
        m_control = data;
        m_temporaryAddress = (m_temporaryAddress & 0xF3FF) |
                             ((static_cast<std::uint16_t>(data) & 0x03) << 10);
        if ((m_status & 0x80) != 0 && (m_control & 0x80) != 0)
        {
            m_nmiPending = true;
        }
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
            m_scrollX = data;
            m_temporaryAddress = (m_temporaryAddress & 0xFFE0) | (data >> 3);
        }
        else
        {
            m_scrollY = data;
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

    const std::uint16_t physicalTable = m_mirroring == Mirroring::Vertical
        ? table & 0x0001
        : table >> 1;
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

std::uint32_t PPU::ColorFromPaletteIndex(std::uint8_t index)
{
    return 0xFF000000 | SystemPalette[index & 0x3F];
}

} // namespace dendyforge
