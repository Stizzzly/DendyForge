#pragma once

#include <cstdint>

namespace dendyforge
{

class CpuBus
{
public:
    virtual ~CpuBus() = default;

    virtual std::uint8_t CpuRead(std::uint16_t address) = 0;
    virtual void CpuWrite(std::uint16_t address, std::uint8_t data) = 0;
};

} // namespace dendyforge
