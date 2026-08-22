#include <cstdint>
#include <filesystem>
#include <iostream>

#include "console/console.hpp"

namespace
{

constexpr std::uint16_t ResultAddress = 0x00F0;
constexpr std::uint8_t PassingResult = 1;
constexpr std::uint64_t CpuClockHz = 1'789'773;
constexpr std::uint64_t MaximumCycles = CpuClockHz * 5;
constexpr std::uint32_t FinalLoopCycles = 64;

bool RunTest(const std::filesystem::path& path)
{
    dendyforge::Console console;
    if (!console.LoadRom(path.string()))
    {
        std::cerr << path << ": could not load ROM\n";
        return false;
    }

    std::uint16_t previousProgramCounter = console.Cpu().ProgramCounter();
    std::uint32_t stableProgramCounterCycles = 0;
    for (std::uint64_t cycle = 0; cycle < MaximumCycles; ++cycle)
    {
        console.Clock();
        const std::uint16_t programCounter = console.Cpu().ProgramCounter();
        stableProgramCounterCycles = programCounter == previousProgramCounter
            ? stableProgramCounterCycles + 1
            : 0;
        previousProgramCounter = programCounter;

        if (stableProgramCounterCycles >= FinalLoopCycles)
        {
            const std::uint8_t result =
                console.ReadCpuRamForDiagnostics(ResultAddress);
            std::cout << path.filename().string() << ": "
                      << (result == PassingResult ? "passed" : "failed")
                      << " (code " << static_cast<int>(result) << ")\n";
            return result == PassingResult;
        }
    }

    std::cerr << path.filename().string()
              << ": timed out before the final loop\n";
    return false;
}

}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: DendyForgeApuTimingRunner <test.nes> [...]\n";
        return 2;
    }

    bool passed = true;
    for (int index = 1; index < argc; ++index)
    {
        passed = RunTest(argv[index]) && passed;
    }
    return passed ? 0 : 1;
}
