#pragma once

#include "mapper.hpp"

namespace dendyforge
{

class Mapper2 : public Mapper
{
public:
    Mapper2(std::uint8_t prgBanks, std::uint8_t chrBanks);

    bool CpuRead(std::uint16_t address,
                 std::uint32_t& mappedAddress) override;
    bool CpuWrite(std::uint16_t address, std::uint8_t data,
                  std::uint32_t& mappedAddress) override;
    bool PpuRead(std::uint16_t address,
                 std::uint32_t& mappedAddress) override;
    bool PpuWrite(std::uint16_t address,
                  std::uint32_t& mappedAddress) override;

private:
    std::uint8_t m_selectedPrgBank{0};
};

} // namespace dendyforge
