#pragma once

#include <cstdint>
#include <vector>

#include "cartridge_info.hpp"

namespace dendyforge
{

class Cartridge
{
public:
    Cartridge(
        const INesHeader& header,
        std::vector<std::uint8_t>&& prgRom,
        std::vector<std::uint8_t>&& chrRom);
        std::uint8_t PRGRomBanks() const;
        std::uint8_t CHRRomBanks() const;

    const CartridgeInfo& Info() const;

    const std::vector<std::uint8_t>& PRGRom() const;

    const std::vector<std::uint8_t>& CHRRom() const;

private:
    CartridgeInfo m_info;

    std::vector<std::uint8_t> m_prgRom;
    std::vector<std::uint8_t> m_chrRom;
};

} // namespace dendyforge
