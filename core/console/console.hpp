#pragma once

#include <cstdint>
#include <memory>
#include <optional>
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
    // Advances PPU/APU/DMA together until the next CPU instruction boundary.
    // A pending DMA or interrupt sequence is included in this operation.
    void StepInstruction();
    bool IsInstructionBoundary() const;
    std::optional<std::uint8_t> DebugPeekCpu(std::uint16_t address);
    std::uint8_t ReadCpuRamForDiagnostics(std::uint16_t address);
    std::uint8_t ReadCartridgeRamForDiagnostics(std::uint16_t address);

    CPU6502& Cpu();
    PPU& VideoProcessor();
    APU& AudioProcessor();
    Controller& PrimaryController();
    Zapper& SecondaryZapper();

private:
    Bus m_bus;
    CPU6502 m_cpu;
    std::unique_ptr<Cartridge> m_cartridge;
    bool m_cpuCycleIsOdd{false};
};

} // namespace dendyforge
