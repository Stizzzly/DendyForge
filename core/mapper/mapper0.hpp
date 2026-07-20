#pragma once

#include "mapper.hpp"

namespace dendyforge
{

class Mapper0 : public Mapper
{
public:
    Mapper0(
        std::uint8_t prgBanks,
        std::uint8_t chrBanks);

    bool CpuRead(
        std::uint16_t address,
        std::uint32_t& mappedAddress) override;

    bool CpuWrite(
        std::uint16_t address,
        std::uint32_t& mappedAddress) override;

    bool PpuRead(
        std::uint16_t address,
        std::uint32_t& mappedAddress) override;

    bool PpuWrite(
        std::uint16_t address,
        std::uint32_t& mappedAddress) override;
};

} // namespace dendyforge
