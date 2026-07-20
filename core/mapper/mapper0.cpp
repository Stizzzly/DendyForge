#include "mapper0.hpp"

namespace dendyforge
{

Mapper0::Mapper0(
    std::uint8_t prgBanks,
    std::uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
}

bool Mapper0::CpuRead(
    std::uint16_t address,
    std::uint32_t& mappedAddress)
{
    if (address >= 0x8000)
    {
        if (m_prgBanks > 1)
        {
            mappedAddress = address & 0x7FFF;
        }
        else
        {
            mappedAddress = address & 0x3FFF;
        }

        return true;
    }

    return false;
}

bool Mapper0::CpuWrite(
    std::uint16_t address,
    std::uint32_t& mappedAddress)
{
    if (address >= 0x8000)
    {
        if (m_prgBanks > 1)
        {
            mappedAddress = address & 0x7FFF;
        }
        else
        {
            mappedAddress = address & 0x3FFF;
        }

        return true;
    }

    return false;
}

bool Mapper0::PpuRead(
    std::uint16_t address,
    std::uint32_t& mappedAddress)
{
    if (address <= 0x1FFF)
    {
        mappedAddress = address;
        return true;
    }

    return false;
}

bool Mapper0::PpuWrite(
    std::uint16_t address,
    std::uint32_t& mappedAddress)
{
    if (address <= 0x1FFF)
    {
        if (m_chrBanks == 0)
        {
            mappedAddress = address;
            return true;
        }
    }

    return false;
}

} // namespace dendyforge
