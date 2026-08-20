#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string_view>

#include "bus/bus.hpp"
#include "cartridge/cartridge.hpp"
#include "cpu/cpu6502.hpp"
#include "ines/ines_reader.hpp"

namespace dendyforge::test
{

inline std::filesystem::path RomPath(std::string_view relativePath)
{
    return std::filesystem::path(DENDYFORGE_SOURCE_DIR) /
           "tests" / "cpu" / "roms" / relativePath;
}

struct CpuMachine
{
    Bus bus;
    Cartridge cartridge;
    CPU6502 cpu;

    CpuMachine(const INesHeader& header,
               std::vector<std::uint8_t>&& prgRom,
               std::vector<std::uint8_t>&& chrRom,
               CPU6502::Configuration cpuConfiguration)
        : cartridge(header, std::move(prgRom), std::move(chrRom)),
          cpu(cpuConfiguration)
    {
        bus.InsertCartridge(&cartridge);
        cpu.ConnectBus(&bus);
        cpu.Reset();
    }
};

inline std::unique_ptr<CpuMachine> LoadCpuMachine(
    std::string_view romPath,
    CPU6502::Configuration cpuConfiguration = {.decimalModeEnabled = false})
{
    INesReader reader;
    const auto path = RomPath(romPath);

    if (!reader.Load(path.string()))
    {
        return nullptr;
    }

    return std::make_unique<CpuMachine>(
        reader.Header(), reader.TakePRGRom(), reader.TakeCHRRom(), cpuConfiguration);
}

inline void CompleteInstruction(CPU6502& cpu)
{
    cpu.Clock();

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }
}

inline void CompleteReset(CPU6502& cpu)
{
    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }
}

inline bool ExecuteUntilSelfJump(CPU6502& cpu, std::size_t maxInstructions = 1'000)
{
    for (std::size_t index = 0; index < maxInstructions; ++index)
    {
        const auto programCounter = cpu.ProgramCounter();
        CompleteInstruction(cpu);

        if (cpu.Opcode() == 0x4C && cpu.ProgramCounter() == programCounter)
        {
            return true;
        }
    }

    return false;
}

} // namespace dendyforge::test
