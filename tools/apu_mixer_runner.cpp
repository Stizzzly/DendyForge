#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

#include "console/console.hpp"

namespace
{

constexpr std::uint8_t TestRunning = 0x80;
constexpr std::uint8_t ResetRequested = 0x81;
constexpr std::uint16_t StatusAddress = 0x6000;
constexpr std::uint16_t TextAddress = 0x6004;
constexpr std::uint64_t CpuClockHz = 1'789'773;
constexpr std::uint64_t ResetDelayCycles = CpuClockHz / 10;
constexpr std::uint64_t MaximumCycles = CpuClockHz * 30;

std::string ReadText(dendyforge::Console& console)
{
    std::string text;
    for (std::uint16_t offset = 0; offset < 256; ++offset)
    {
        const std::uint8_t character =
            console.ReadCartridgeRamForDiagnostics(TextAddress + offset);
        if (character == 0)
        {
            break;
        }

        text += static_cast<char>(character);
    }
    return text;
}

bool RunTest(const std::filesystem::path& path)
{
    dendyforge::Console console;
    if (!console.LoadRom(path.string()))
    {
        std::cerr << path << ": could not load ROM\n";
        return false;
    }

    bool started = false;
    bool resetPending = false;
    std::uint64_t resetRequestedAt = 0;
    for (std::uint64_t cycle = 0; cycle < MaximumCycles; ++cycle)
    {
        console.Clock();
        const std::uint8_t status =
            console.ReadCartridgeRamForDiagnostics(StatusAddress);

        if (status == TestRunning || status == ResetRequested)
        {
            started = true;
        }

        if (status == ResetRequested && !resetPending)
        {
            resetPending = true;
            resetRequestedAt = cycle;
        }

        if (resetPending && cycle - resetRequestedAt >= ResetDelayCycles)
        {
            console.Reset();
            resetPending = false;
        }

        if (started && status < TestRunning)
        {
            std::cout << path.filename().string() << ": "
                      << (status == 0 ? "passed" : "failed")
                      << " (code " << static_cast<int>(status) << ")";
            const std::string text = ReadText(console);
            if (!text.empty())
            {
                std::cout << " — " << text;
            }
            std::cout << '\n';
            return status == 0;
        }
    }

    std::cerr << path.filename().string()
              << ": timed out before a final status\n";
    return false;
}

}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: DendyForgeApuMixerRunner <test.nes> [...]\n";
        return 2;
    }

    bool passed = true;
    for (int index = 1; index < argc; ++index)
    {
        passed = RunTest(argv[index]) && passed;
    }
    return passed ? 0 : 1;
}
