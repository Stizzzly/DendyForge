#include "mapper1.hpp"

namespace dendyforge
{

Mapper1::Mapper1(std::uint8_t prgBanks, std::uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
}

bool Mapper1::CpuRead(std::uint16_t address,
                      std::uint32_t& mappedAddress)
{
    if (address < 0x8000 || m_prgBanks == 0)
    {
        return false;
    }

    const std::uint8_t prgMode = (m_control >> 2) & 0x03;
    std::uint8_t bank = 0;

    if (prgMode <= 1)
    {
        // 32 KiB mode: the low bit of the bank register is ignored and
        // the two windows hold adjacent banks.
        const std::uint8_t base = static_cast<std::uint8_t>(m_prgBank & 0x0E);
        bank = address < 0xC000
            ? base
            : static_cast<std::uint8_t>(base | 0x01);
    }
    else if (prgMode == 2)
    {
        bank = address < 0xC000
            ? 0
            : static_cast<std::uint8_t>(m_prgBank & 0x0F);
    }
    else
    {
        bank = address < 0xC000
            ? static_cast<std::uint8_t>(m_prgBank & 0x0F)
            : static_cast<std::uint8_t>(m_prgBanks - 1);
    }

    bank = static_cast<std::uint8_t>(bank % m_prgBanks);
    mappedAddress = static_cast<std::uint32_t>(bank) * 0x4000 |
                    (address & 0x3FFF);
    return true;
}

bool Mapper1::CpuWrite(std::uint16_t address, std::uint8_t data,
                       std::uint32_t& mappedAddress)
{
    if (address < 0x8000 || m_prgBanks == 0)
    {
        return false;
    }

    if (data & 0x80)
    {
        // A write with bit 7 set resets the shift sequence and forces
        // PRG bank mode 3; the CHR bank mode and nametable arrangement
        // are not altered.
        m_shiftRegister = 0;
        m_writeCount = 0;
        m_control |= 0x0C;
        mappedAddress = NoMappedAddress;
        return true;
    }

    // The first written bit becomes bit 0 of the register and the fifth
    // becomes bit 4: games send the five-bit value LSB first.
    if (m_writeCount < 4)
    {
        m_shiftRegister = static_cast<std::uint8_t>(
            (m_shiftRegister >> 1) | ((data & 0x01) << 4));
        ++m_writeCount;
    }
    else
    {
        const std::uint8_t value = static_cast<std::uint8_t>(
            (m_shiftRegister >> 1) | ((data & 0x01) << 4));

        switch ((address >> 13) & 0x03)
        {
        case 0:
            LoadControl(value);
            break;
        case 1:
            m_chrBank0 = value;
            break;
        case 2:
            m_chrBank1 = value;
            break;
        case 3:
            m_prgBank = value;
            break;
        default:
            break;
        }

        m_shiftRegister = 0;
        m_writeCount = 0;
    }

    mappedAddress = NoMappedAddress;
    return true;
}

bool Mapper1::PpuRead(std::uint16_t address,
                      std::uint32_t& mappedAddress)
{
    if (address > 0x1FFF)
    {
        return false;
    }

    if (m_chrBanks == 0)
    {
        // Boards with CHR RAM (SNROM family) are not CHR-banked.
        mappedAddress = address;
        return true;
    }

    if ((m_control & 0x10) == 0)
    {
        // 8 KiB CHR mode: the low bit of CHR bank 0 is ignored and the
        // CHR bank 1 register is ignored.
        const std::uint8_t bank8 = static_cast<std::uint8_t>(
            ((m_chrBank0 & 0x1E) >> 1) % m_chrBanks);
        mappedAddress = static_cast<std::uint32_t>(bank8) * 0x2000 |
                        (address & 0x1FFF);
    }
    else
    {
        const std::uint8_t bank4 = static_cast<std::uint8_t>(
            ((address < 0x1000 ? m_chrBank0 : m_chrBank1) & 0x1F) %
            static_cast<std::uint8_t>(m_chrBanks * 2));
        mappedAddress = static_cast<std::uint32_t>(bank4) * 0x1000 |
                        (address & 0x0FFF);
    }

    return true;
}

bool Mapper1::PpuWrite(std::uint16_t address,
                       std::uint32_t& mappedAddress)
{
    if (address > 0x1FFF || m_chrBanks != 0)
    {
        return false;
    }

    mappedAddress = address;
    return true;
}

Mirroring Mapper1::MirroringMode(Mirroring headerMirroring) const
{
    if (!m_controlLoaded)
    {
        return headerMirroring;
    }

    switch (m_control & 0x03)
    {
    case 0:
        return Mirroring::OneScreenLower;
    case 1:
        return Mirroring::OneScreenUpper;
    case 2:
        // Horizontal arrangement of the two nametables, i.e. PPU A10
        // controls the table: this project calls that Vertical.
        return Mirroring::Vertical;
    default:
        return Mirroring::Horizontal;
    }
}

void Mapper1::LoadControl(std::uint8_t value)
{
    m_control = value;
    m_controlLoaded = true;
}

} // namespace dendyforge
