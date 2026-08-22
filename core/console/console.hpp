#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "bus/bus.hpp"
#include "cpu/cpu6502.hpp"

namespace dendyforge
{

class Cartridge;

class Console
{
public:
    Console();
    ~Console();

    bool LoadRom(const std::string& path);
    void Reset();
    void Clock();
    std::uint8_t ReadCpuRamForDiagnostics(std::uint16_t address);
    std::uint8_t ReadCartridgeRamForDiagnostics(std::uint16_t address);

    CPU6502& Cpu();
    PPU& VideoProcessor();
    APU& AudioProcessor();
    Controller& PrimaryController();

private:
    Bus m_bus;
    CPU6502 m_cpu;
    std::unique_ptr<Cartridge> m_cartridge;
    bool m_cpuCycleIsOdd{false};
};

} // namespace dendyforge
