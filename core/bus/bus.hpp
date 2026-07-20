#pragma once

#include <array>
#include <cstdint>

namespace dendyforge
{

class Cartridge;

class Bus
{
public:
    Bus();

    void InsertCartridge(Cartridge* cartridge);

    std::uint8_t CpuRead(std::uint16_t address);
    void CpuWrite(std::uint16_t address, std::uint8_t data);

private:
    Cartridge* m_cartridge{nullptr};

    std::array<std::uint8_t, 2048> m_cpuRam{};
};

}
