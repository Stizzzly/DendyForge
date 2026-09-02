#include "mapper7.hpp"

namespace dendyforge
{

Mapper7::Mapper7(std::uint8_t prgBanks, std::uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
}

bool Mapper7::CpuRead(std::uint16_t address,
                      std::uint32_t& mappedAddress)
{
    if (address < 0x8000 || m_prgBanks < 2)
    {
        return false;
    }

    const std::uint8_t bankCount = m_prgBanks / 2;
    const std::uint8_t bank = static_cast<std::uint8_t>(m_prgBank % bankCount);
    mappedAddress = static_cast<std::uint32_t>(bank) * 0x8000 |
                    (address & 0x7FFF);
    return true;
}

bool Mapper7::CpuWrite(std::uint16_t address, std::uint8_t data,
                       std::uint32_t& mappedAddress)
{
    if (address < 0x8000 || m_prgBanks < 2)
    {
        return false;
    }

    m_prgBank = data & 0x07;
    m_mirroring = (data & 0x10) != 0
        ? Mirroring::OneScreenUpper
        : Mirroring::OneScreenLower;
    mappedAddress = NoMappedAddress;
    return true;
}

bool Mapper7::PpuRead(std::uint16_t address,
                      std::uint32_t& mappedAddress)
{
    if (address > 0x1FFF)
    {
        return false;
    }

    mappedAddress = address;
    return true;
}

bool Mapper7::PpuWrite(std::uint16_t address,
                       std::uint32_t& mappedAddress)
{
    if (address > 0x1FFF || m_chrBanks != 0)
    {
        return false;
    }

    mappedAddress = address;
    return true;
}

Mirroring Mapper7::MirroringMode(Mirroring) const
{
    return m_mirroring;
}

} // namespace dendyforge
