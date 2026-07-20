#pragma once

#include <cstdint>

namespace dendyforge
{

class Mapper
{
public:
    Mapper(std::uint8_t prgBanks, std::uint8_t chrBanks);
    virtual ~Mapper() = default;

    virtual bool CpuRead(
        std::uint16_t address,
        std::uint32_t& mappedAddress) = 0;

    virtual bool CpuWrite(
        std::uint16_t address,
        std::uint32_t& mappedAddress) = 0;

    virtual bool PpuRead(
        std::uint16_t address,
        std::uint32_t& mappedAddress) = 0;

    virtual bool PpuWrite(
        std::uint16_t address,
        std::uint32_t& mappedAddress) = 0;

protected:
    std::uint8_t m_prgBanks;
    std::uint8_t m_chrBanks;
};

} // namespace dendyforge
