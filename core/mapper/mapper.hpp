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

    // The PPU reports its address bus on every memory access and advances
    // mapper timing once per dot. MMC3 uses these to qualify rising A12
    // edges; a scanline callback is not accurate enough for split-scroll
    // and nonstandard pattern-table use.
    virtual void PpuClock();
    virtual void ObservePpuAddress(std::uint16_t address);
    [[nodiscard]] virtual bool MonitorsPpuBus() const noexcept;

    // PRG-RAM control. Most mappers leave RAM continuously enabled and
    // writable; MMC3 exposes the $A001 enable/write-protect bits.
    virtual bool PrgRamEnabled() const;
    virtual bool PrgRamWriteProtected() const;

    // The mapper's request for a CPU IRQ line (level; MMC3 until
    // acknowledged).
    virtual bool IrqPending() const;

protected:
    std::uint8_t m_prgBanks;
    std::uint8_t m_chrBanks;
};

} // namespace dendyforge
