#include "cartridge.hpp"

#include <utility>

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

std::uint8_t Cartridge::PRGRomBanks() const
{
    return static_cast<std::uint8_t>(m_prgRom.size() / (16 * 1024));
}

std::uint8_t Cartridge::CHRRomBanks() const
{
    return static_cast<std::uint8_t>(m_chrRom.size() / (8 * 1024));
}

} // namespace dendyforge
