#include "ppu.hpp"

#include "cartridge/cartridge.hpp"

namespace dendyforge
{

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
        m_status &= ~0x80;
        m_nmiPending = false;
    }
    else if (m_scanline == 241 && m_cycle == 1)
    {
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

    if (address <= 0x1FFF && m_cartridge && m_cartridge->PpuRead(address, data))
    {
        return data;
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

    if (address <= 0x1FFF && m_cartridge && m_cartridge->PpuWrite(address, data))
    {
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

} // namespace dendyforge
