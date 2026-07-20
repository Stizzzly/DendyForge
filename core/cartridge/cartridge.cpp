#include "cartridge.hpp"
#include "../mapper/mapper0.hpp"

#include <utility>
#include <stdexcept>

namespace dendyforge
{

Cartridge::Cartridge(
    const INesHeader& header,
    std::vector<std::uint8_t>&& prgRom,
    std::vector<std::uint8_t>&& chrRom)
    : m_info(header),
      m_prgRom(std::move(prgRom)),
      m_chrRom(std::move(chrRom))
{
    switch (m_info.Mapper())
    {
    case 0:
        m_mapper = std::make_unique<Mapper0>(
            PRGRomBanks(),
            CHRRomBanks());
        break;

    default:
        throw std::runtime_error("Unsupported mapper");
    }
}

const CartridgeInfo& Cartridge::Info() const
{
    return m_info;
}

const std::vector<std::uint8_t>& Cartridge::PRGRom() const
{
    return m_prgRom;
}

const std::vector<std::uint8_t>& Cartridge::CHRRom() const
{
    return m_chrRom;
}

bool Cartridge::CpuRead(
    std::uint16_t address,
    std::uint8_t& data)
{
    std::uint32_t mappedAddress;

    if (!m_mapper->CpuRead(address, mappedAddress))
    {
        return false;
    }

    if (mappedAddress >= m_prgRom.size())
{
    return false;
}

    data = m_prgRom[mappedAddress];
    return true;
}

bool Cartridge::CpuWrite(
    std::uint16_t address,
    std::uint8_t data)
{
    std::uint32_t mappedAddress;

    if (!m_mapper->CpuWrite(address, mappedAddress))
    {
        return false;
    }

    if (mappedAddress >= m_prgRom.size())
    {
        return false;
    }

    m_prgRom[mappedAddress] = data;
    return true;
}

bool Cartridge::PpuRead(
    std::uint16_t address,
    std::uint8_t& data)
{
    std::uint32_t mappedAddress;

    if (!m_mapper->PpuRead(address, mappedAddress))
    {
        return false;
    }

    if (mappedAddress >= m_chrRom.size())
    {
        return false;
    }

    data = m_chrRom[mappedAddress];
    return true;
}

bool Cartridge::PpuWrite(
    std::uint16_t address,
    std::uint8_t data)
{
    std::uint32_t mappedAddress;

    if (!m_mapper->PpuWrite(address, mappedAddress))
    {
        return false;
    }

    if (mappedAddress >= m_chrRom.size())
    {
        return false;
    }

    m_chrRom[mappedAddress] = data;
    return true;
}

std::uint8_t Cartridge::PRGRomBanks() const
{
    return static_cast<std::uint8_t>(m_prgRom.size() / (16 * 1024));
}

std::uint8_t Cartridge::CHRRomBanks() const
{
    return static_cast<std::uint8_t>(m_chrRom.size() / (8 * 1024));
}


} // namespace dendyforge
