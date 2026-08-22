#include "mapper3.hpp"

namespace dendyforge
{

Mapper3::Mapper3(std::uint8_t prgBanks, std::uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
}

bool Mapper3::CpuRead(std::uint16_t address,
                      std::uint32_t& mappedAddress)
{
    if (address < 0x8000 || m_prgBanks == 0)
    {
        return false;
    }

    // A single 16 KiB PRG bank mirrors through the whole window like
    // NROM-128; otherwise the full 32 KiB window is mapped.
    mappedAddress = m_prgBanks == 1 ? address & 0x3FFF
                                    : address & 0x7FFF;
    return true;
}

bool Mapper3::CpuWrite(std::uint16_t address, std::uint8_t data,
                       std::uint32_t& mappedAddress)
{
    if (address < 0x8000)
    {
        return false;
    }

    // Only the two low CHR bank lines exist; the remaining data bits are
    // ignored (they drive copy-protection on some boards).
    m_chrBank = data & 0x03;
    mappedAddress = NoMappedAddress;
    return true;
}

bool Mapper3::PpuRead(std::uint16_t address,
                      std::uint32_t& mappedAddress)
{
    if (address > 0x1FFF)
    {
        return false;
    }

    const std::uint8_t banks = m_chrBanks == 0 ? 1 : m_chrBanks;
    const std::uint8_t bank = static_cast<std::uint8_t>(
        m_chrBank % banks);
    mappedAddress = static_cast<std::uint32_t>(bank) * 0x2000 |
                    (address & 0x1FFF);
    return true;
}

bool Mapper3::PpuWrite(std::uint16_t address,
                       std::uint32_t& mappedAddress)
{
    // CHR ROM is not writable; boards with CHR RAM report no mapping and
    // let the cartridge's fallback CHR RAM absorb the write.
    if (m_chrBanks != 0)
    {
        return false;
    }

    mappedAddress = address;
    return true;
}

} // namespace dendyforge
