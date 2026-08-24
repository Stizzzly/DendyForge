#pragma once

#include <cstdint>
#include <vector>

#include "cartridge_info.hpp"

#include <memory>

#include "../mapper/mapper.hpp"

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

    // Current nametable arrangement: the iNES header mode unless the
    // mapper switches it at runtime (MMC1).
    Mirroring CurrentMirroring() const;

    // Mapper-visible PPU bus timing (MMC3 uses qualified A12 rises for
    // its IRQ counter).
    void PpuClock();
    void ObservePpuAddress(std::uint16_t address);
    bool IrqPending() const;

    bool CpuRead(std::uint16_t address, std::uint8_t& data);
    bool CpuWrite(std::uint16_t address, std::uint8_t data);

    bool PpuRead(std::uint16_t address, std::uint8_t& data);
    bool PpuWrite(std::uint16_t address, std::uint8_t data);

    const std::vector<std::uint8_t>& PRGRom() const;

    const std::vector<std::uint8_t>& CHRRom() const;

private:
    CartridgeInfo m_info;

    std::vector<std::uint8_t> m_prgRom;
    std::vector<std::uint8_t> m_prgRam;
    std::vector<std::uint8_t> m_chrRom;
    std::vector<std::uint8_t> m_chrRam;
    std::unique_ptr<Mapper> m_mapper;
};

} // namespace dendyforge
