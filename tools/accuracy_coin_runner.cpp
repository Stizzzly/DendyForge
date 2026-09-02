#include <array>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>

#include "console/console.hpp"

// AccuracyCoin is an interactive ROM. Pressing Start on the top-level menu
// runs all 141 pass/fail tests. Results live in CPU RAM $0400-$0492: bit 0
// denotes pass, while a failing error code is stored in bits 7..2. This runner
// drives that documented UI path and reports the raw result addresses so a
// failure can be matched to the labels in AccuracyCoin.asm.

namespace
{

constexpr std::uint16_t MenuBootProgress = 0x00EC;
constexpr std::uint16_t RunningAllTests = 0x0035;
constexpr std::uint16_t TestsRun = 0x0037;
constexpr std::uint16_t FirstResult = 0x0400;
constexpr std::uint16_t LastResult = 0x0492;
constexpr std::uint8_t MenuReady = 0x0A;
constexpr std::uint64_t CpuClockHz = 1'789'773;
constexpr std::uint64_t MaximumCycles = CpuClockHz * 10 * 60;
constexpr std::uint64_t MaximumCyclesWithoutProgress = CpuClockHz * 60;
constexpr std::uint64_t StartPressCycles = CpuClockHz / 10;

void PrintFailures(dendyforge::Console& console)
{
    for (std::uint16_t address = FirstResult; address <= LastResult; ++address)
    {
        const std::uint8_t result = console.ReadCpuRamForDiagnostics(address);
        if (result != 0 && (result & 1) == 0)
        {
            std::cout << "$" << std::uppercase << std::hex
                      << std::setw(4) << std::setfill('0') << address
                      << ": code " << static_cast<unsigned>(result >> 2)
                      << " (raw $" << std::setw(2)
                      << static_cast<unsigned>(result) << ")\n";
        }
    }
    std::cout << std::dec;
}

std::uint8_t CountPassed(dendyforge::Console& console)
{
    std::uint8_t passed = 0;
    for (std::uint16_t address = FirstResult; address <= LastResult; ++address)
    {
        passed = static_cast<std::uint8_t>(
            passed + ((console.ReadCpuRamForDiagnostics(address) & 1) != 0));
    }
    return passed;
}

struct DiagnosticSnapshot
{
    std::uint16_t resultAddress;
    std::uint16_t dataAddress;
    const char* label;
    bool captured{false};
    std::array<std::uint8_t, 0x60> bytes{};
};

struct OamStressDiagnostic
{
    bool captured{false};
    std::uint8_t x{0};
    std::uint8_t y{0};
    std::uint16_t expectedAddress{0};
    std::uint8_t expected{0};
    std::uint8_t actualPage5{0};
    std::uint8_t actualPage6{0};
    std::uint8_t alignmentShifted{0};
};

struct StatusTimingDiagnostic
{
    bool captured{false};
    std::array<std::uint8_t, 4> clearSamples{};
};

int Run(const std::filesystem::path& path)
{
    dendyforge::Console console;
    if (!console.LoadRom(path.string()))
    {
        std::cerr << "Could not load " << path << '\n';
        return 2;
    }

    bool pressedStart = false;
    std::uint64_t pressedStartAt = 0;
    bool started = false;
    std::uint8_t lastProgress = 0;
    std::uint64_t lastProgressAt = 0;
    std::array<DiagnosticSnapshot, 5> snapshots{{
        {0x0462, 0x0500, "NMI overlap BRK"},
        {0x0463, 0x0500, "NMI overlap IRQ"},
        {0x046B, 0x0500, "DMC DMA bus conflicts"},
        {0x0478, 0x0500, "implicit DMA abort"},
        {0x0479, 0x0050, "explicit DMA abort"},
    }};
    OamStressDiagnostic oamStressDiagnostic;
    StatusTimingDiagnostic statusTimingDiagnostic;

    for (std::uint64_t cycle = 0; cycle < MaximumCycles; ++cycle)
    {
        console.Clock();

        for (auto& snapshot : snapshots)
        {
            if (!snapshot.captured &&
                console.ReadCpuRamForDiagnostics(snapshot.resultAddress) != 0)
            {
                snapshot.captured = true;
                for (std::size_t index = 0; index < snapshot.bytes.size(); ++index)
                {
                    snapshot.bytes[index] = console.ReadCpuRamForDiagnostics(
                        static_cast<std::uint16_t>(snapshot.dataAddress + index));
                }
            }
        }

        if (!oamStressDiagnostic.captured &&
            console.ReadCpuRamForDiagnostics(0x048C) != 0)
        {
            oamStressDiagnostic.captured = true;
            oamStressDiagnostic.x = console.ReadCpuRamForDiagnostics(0x0020);
            oamStressDiagnostic.y = console.ReadCpuRamForDiagnostics(0x0021);
            oamStressDiagnostic.expectedAddress =
                console.ReadCpuRamForDiagnostics(0x0060) |
                (static_cast<std::uint16_t>(
                     console.ReadCpuRamForDiagnostics(0x0061)) << 8);
            oamStressDiagnostic.expected = console.DebugPeekCpu(
                static_cast<std::uint16_t>(oamStressDiagnostic.expectedAddress +
                                           oamStressDiagnostic.y)).value_or(0);
            oamStressDiagnostic.actualPage5 =
                console.ReadCpuRamForDiagnostics(
                    static_cast<std::uint16_t>(0x0500 + oamStressDiagnostic.y));
            oamStressDiagnostic.actualPage6 =
                console.ReadCpuRamForDiagnostics(
                    static_cast<std::uint16_t>(0x0600 + oamStressDiagnostic.y));
            oamStressDiagnostic.alignmentShifted =
                console.ReadCpuRamForDiagnostics(0x006F);
        }

        if (!statusTimingDiagnostic.captured &&
            console.ReadCpuRamForDiagnostics(0x048D) != 0)
        {
            statusTimingDiagnostic.captured = true;
            for (std::size_t index = 0;
                 index < statusTimingDiagnostic.clearSamples.size(); ++index)
            {
                statusTimingDiagnostic.clearSamples[index] =
                    console.ReadCpuRamForDiagnostics(
                        static_cast<std::uint16_t>(0x006C + index));
            }
        }

        if (!pressedStart &&
            console.ReadCpuRamForDiagnostics(MenuBootProgress) == MenuReady)
        {
            console.PrimaryController().SetButton(
                dendyforge::Controller::Button::Start, true);
            pressedStart = true;
            pressedStartAt = cycle;
        }

        if (pressedStart && cycle - pressedStartAt == StartPressCycles)
        {
            console.PrimaryController().SetButton(
                dendyforge::Controller::Button::Start, false);
        }

        const std::uint8_t running =
            console.ReadCpuRamForDiagnostics(RunningAllTests);
        if (pressedStart && running != 0)
        {
            started = true;
        }

        const std::uint8_t progress =
            console.ReadCpuRamForDiagnostics(TestsRun);
        if (started && progress != lastProgress)
        {
            lastProgress = progress;
            lastProgressAt = cycle;
            std::cout << "\rRunning AccuracyCoin: "
                      << static_cast<unsigned>(progress) << "/141"
                      << std::flush;
        }

        if (started && lastProgress == 141 && running == 0)
        {
            const std::uint8_t passed = CountPassed(console);
            std::cout << "\rAccuracyCoin: "
                      << static_cast<unsigned>(passed)
                      << "/"
                      << static_cast<unsigned>(lastProgress)
                      << " passed\n";
            PrintFailures(console);
            if (oamStressDiagnostic.captured &&
                (console.ReadCpuRamForDiagnostics(0x048C) & 1) == 0)
            {
                std::cout << "$2004 stress first mismatch: X=$"
                          << std::uppercase << std::hex << std::setw(2)
                          << std::setfill('0')
                          << static_cast<unsigned>(oamStressDiagnostic.x)
                          << " Y=$" << std::setw(2)
                          << static_cast<unsigned>(oamStressDiagnostic.y)
                          << " expected[$" << std::setw(4)
                          << oamStressDiagnostic.expectedAddress << "+Y]=$"
                          << std::setw(2)
                          << static_cast<unsigned>(oamStressDiagnostic.expected)
                          << " actual $500+Y=$" << std::setw(2)
                          << static_cast<unsigned>(oamStressDiagnostic.actualPage5)
                          << " $600+Y=$" << std::setw(2)
                          << static_cast<unsigned>(oamStressDiagnostic.actualPage6)
                          << " shifted=$" << std::setw(2)
                          << static_cast<unsigned>(
                                 oamStressDiagnostic.alignmentShifted)
                          << std::dec << '\n';
            }
            if (statusTimingDiagnostic.captured &&
                (console.ReadCpuRamForDiagnostics(0x048D) & 1) == 0)
            {
                std::cout << "$2002 clear timing samples:";
                for (const std::uint8_t sample :
                     statusTimingDiagnostic.clearSamples)
                {
                    std::cout << " $" << std::uppercase << std::hex
                              << std::setw(2) << std::setfill('0')
                              << static_cast<unsigned>(sample);
                }
                std::cout << std::dec << '\n';
            }
            for (const auto& snapshot : snapshots)
            {
                if (!snapshot.captured ||
                    (console.ReadCpuRamForDiagnostics(snapshot.resultAddress) & 1) != 0)
                {
                    continue;
                }
                std::cout << snapshot.label << " diagnostic snapshot:\n";
                for (std::size_t index = 0; index < snapshot.bytes.size(); ++index)
                {
                    if ((index & 0x0F) == 0)
                    {
                        std::cout << "$" << std::uppercase << std::hex
                                  << std::setw(4) << std::setfill('0')
                                  << snapshot.dataAddress + index << ":";
                    }
                    std::cout << " " << std::setw(2)
                              << static_cast<unsigned>(snapshot.bytes[index]);
                    if ((index & 0x0F) == 0x0F)
                    {
                        std::cout << '\n';
                    }
                }
                std::cout << std::dec;
            }
            return passed == lastProgress ? 0 : 1;
        }

        if (started && cycle - lastProgressAt >= MaximumCyclesWithoutProgress)
        {
            std::cerr << "\nAccuracyCoin stalled after "
                      << static_cast<unsigned>(lastProgress)
                      << "/141 tests at PC $" << std::uppercase << std::hex
                      << std::setw(4) << std::setfill('0')
                      << console.Cpu().ProgramCounter() << std::dec << '\n';
            PrintFailures(console);
            return 2;
        }
    }

    std::cerr << "AccuracyCoin timed out (boot progress $" << std::hex
              << static_cast<unsigned>(
                     console.ReadCpuRamForDiagnostics(MenuBootProgress))
              << ", completed " << std::dec
              << static_cast<unsigned>(
                     console.ReadCpuRamForDiagnostics(TestsRun))
              << "/141 tests)\n";
    return 2;
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: DendyForgeAccuracyCoinRunner <AccuracyCoin.nes>\n";
        return 2;
    }
    return Run(argv[1]);
}
