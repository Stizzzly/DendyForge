#pragma once

#include "mapper.hpp"

namespace dendyforge
{

// AxROM (iNES mapper 7). A write anywhere in $8000-$FFFF selects one
// 32 KiB PRG bank and one of the two single-screen nametables. AxROM boards
// use a fixed 8 KiB CHR RAM window.
class Mapper7 : public Mapper
{
public:
    Mapper7(std::uint8_t prgBanks, std::uint8_t chrBanks);

    bool CpuRead(std::uint16_t address,
                 std::uint32_t& mappedAddress) override;
    bool CpuWrite(std::uint16_t address, std::uint8_t data,
                  std::uint32_t& mappedAddress) override;
    bool PpuRead(std::uint16_t address,
                 std::uint32_t& mappedAddress) override;
    bool PpuWrite(std::uint16_t address,
                  std::uint32_t& mappedAddress) override;
    Mirroring MirroringMode(Mirroring headerMirroring) const override;

private:
    std::uint8_t m_prgBank{0};
    Mirroring m_mirroring{Mirroring::OneScreenLower};
};

} // namespace dendyforge
