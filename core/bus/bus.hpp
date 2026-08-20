#pragma once

#include <array>
#include <cstdint>

#include "../cpu/cpu_bus.hpp"

namespace dendyforge
{

class Cartridge;

class Bus : public CpuBus
{
public:
    Bus();

    void InsertCartridge(Cartridge* cartridge);

    std::uint8_t CpuRead(std::uint16_t address) override;
    void CpuWrite(std::uint16_t address, std::uint8_t data) override;

private:
    Cartridge* m_cartridge{nullptr};

    std::array<std::uint8_t, 2048> m_cpuRam{};
};

}
