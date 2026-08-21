#include "mapper2.hpp"

namespace dendyforge
{

Mapper2::Mapper2(std::uint8_t prgBanks, std::uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
}

bool Mapper2::CpuRead(std::uint16_t address, std::uint32_t& mappedAddress)
{
    if (address < 0x8000 || m_prgBanks == 0)
    {
        return false;
    }

    const std::uint8_t bank = address < 0xC000
        ? m_selectedPrgBank
        : static_cast<std::uint8_t>(m_prgBanks - 1);
    mappedAddress = static_cast<std::uint32_t>(bank) * 0x4000 |
                    (address & 0x3FFF);
    return true;
}

bool Mapper2::CpuWrite(std::uint16_t address, std::uint8_t data,
                       std::uint32_t& mappedAddress)
{
    if (address < 0x8000 || m_prgBanks == 0)
    {
        return false;
    }

    m_selectedPrgBank = data % m_prgBanks;
    mappedAddress = NoMappedAddress;
    return true;
}

bool Mapper2::PpuRead(std::uint16_t address, std::uint32_t& mappedAddress)
{
    if (address > 0x1FFF)
    {
        return false;
    }

    mappedAddress = address;
    return true;
}

bool Mapper2::PpuWrite(std::uint16_t address, std::uint32_t& mappedAddress)
{
    if (address > 0x1FFF || m_chrBanks != 0)
    {
        return false;
    }

    mappedAddress = address;
    return true;
}

} // namespace dendyforge
