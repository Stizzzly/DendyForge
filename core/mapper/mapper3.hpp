#pragma once

#include "mapper.hpp"

namespace dendyforge
{

// CNROM (iNES mapper 3). PRG is fixed like NROM; a write anywhere in
// $8000-$FFFF selects one 8 KiB CHR bank.
class Mapper3 : public Mapper
{
public:
    Mapper3(std::uint8_t prgBanks, std::uint8_t chrBanks);

    bool CpuRead(std::uint16_t address,
                 std::uint32_t& mappedAddress) override;
    bool CpuWrite(std::uint16_t address, std::uint8_t data,
                  std::uint32_t& mappedAddress) override;
    bool PpuRead(std::uint16_t address,
                 std::uint32_t& mappedAddress) override;
    bool PpuWrite(std::uint16_t address,
                  std::uint32_t& mappedAddress) override;

private:
    std::uint8_t m_chrBank{0};
};

} // namespace dendyforge
