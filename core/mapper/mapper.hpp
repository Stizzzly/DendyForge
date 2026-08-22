#pragma once

#include <cstdint>

#include "ines/ines.hpp"

namespace dendyforge
{

class Mapper
{
public:
    static constexpr std::uint32_t NoMappedAddress = 0xFFFFFFFF;

    Mapper(std::uint8_t prgBanks, std::uint8_t chrBanks);
    virtual ~Mapper() = default;

    virtual bool CpuRead(
        std::uint16_t address,
        std::uint32_t& mappedAddress) = 0;

    virtual bool CpuWrite(
        std::uint16_t address,
        std::uint8_t data,
        std::uint32_t& mappedAddress) = 0;

    virtual bool PpuRead(
        std::uint16_t address,
        std::uint32_t& mappedAddress) = 0;

    virtual bool PpuWrite(
        std::uint16_t address,
        std::uint32_t& mappedAddress) = 0;

    // Mappers with switchable nametable arrangement (e.g. MMC1) override
    // this; the default keeps the arrangement from the iNES header.
    virtual Mirroring MirroringMode(Mirroring headerMirroring) const;

    // Mappers with a scanline IRQ counter (MMC3) are clocked once per
    // rendering scanline by the PPU sprite-fetch phase.
    virtual void PpuScanlineClock();
    // The mapper's request for a CPU IRQ line (level; MMC3 until
    // acknowledged).
    virtual bool IrqPending() const;

protected:
    std::uint8_t m_prgBanks;
    std::uint8_t m_chrBanks;
};

} // namespace dendyforge
