#include <array>
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

// The per-cycle CPU changes PC inside an instruction, so the ROM's final
// loop shows up as a short, exactly periodic PC sequence rather than a
// constant PC. For each period up to eight cycles, count how many
// consecutive cycles the sequence has repeated with that period; blargg's
// nested delay loops never sustain such a period for more than ~1300
// cycles, while the final loop repeats forever.
constexpr std::size_t MaximumPeriod = 8;
constexpr std::uint32_t StablePeriodCycles = 2048;

bool RunTest(const std::filesystem::path& path)
{
    dendyforge::Console console;
    if (!console.LoadRom(path.string()))
    {
        std::cerr << path << ": could not load ROM\n";
        return false;
    }

    std::array<std::uint16_t, MaximumPeriod> recentProgramCounters{};
    std::array<std::uint32_t, MaximumPeriod + 1> periodStreaks{};
    for (std::uint64_t cycle = 0; cycle < MaximumCycles; ++cycle)
    {
        console.Clock();
        const std::uint16_t programCounter = console.Cpu().ProgramCounter();
        const std::size_t slot = cycle % MaximumPeriod;
        for (std::size_t period = 2; period <= MaximumPeriod; ++period)
        {
            const std::size_t previous = (cycle + MaximumPeriod - period) %
                                         MaximumPeriod;
            periodStreaks[period] =
                cycle >= period && programCounter == recentProgramCounters[previous]
                    ? periodStreaks[period] + 1
                    : 0;
        }
        recentProgramCounters[slot] = programCounter;

        if (periodStreaks[2] >= StablePeriodCycles ||
            periodStreaks[3] >= StablePeriodCycles ||
            periodStreaks[4] >= StablePeriodCycles ||
            periodStreaks[5] >= StablePeriodCycles ||
            periodStreaks[6] >= StablePeriodCycles ||
            periodStreaks[7] >= StablePeriodCycles ||
            periodStreaks[8] >= StablePeriodCycles)
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
