
#include <array>
#include <cstdint>
#include <stdexcept>
#include <filesystem>
#include <iostream>
#include <string>

#include "console/console.hpp"

// Headless runner for blargg PPU test-ROM suites. The local suites use two
// reporting protocols, auto-detected per ROM:
//
// * PRG-RAM protocol (oam_read, oam_stress, ppu_open_bus, ppu_read_buffer,
//   sprdma_and_dmc_dma, ppu_vbl_nmi): $6000 == $80 while running, may
//   request a reset with $81, and a final value below $80 where 0 is a
//   pass; diagnostic text is NUL-terminated at $6004.
// * Zero-page protocol (blargg_ppu_tests_2005, vbl_nmi_timing,
//   sprite_hit_tests_2005, sprite_overflow_tests): no memory handshake;
//   the ROM prints its report and ends in a `jmp self` loop. The per-cycle
//   CPU advances PC inside an instruction, so that loop shows up as a
//   short, exactly periodic PC sequence (periods up to eight), and so do
//   the suites' other loops: $2002 polling (period ~7) holds for at most
//   one frame (~29800 cycles) and nested delay loops for at most ~1300
//   cycles. Requiring a period-2..8 sequence sustained for 40000 cycles
//   (longer than one frame) leaves only the final loop. The result code
//   then sits in $00F8 (2010-era framework) or $00F0 (2005 framework);
//   both bytes double as scratch during the run, so they are only read
//   once the final loop is confirmed. Code 1 is a pass.
//
// Completion detection uses emulated cycles only; the timeout is 60
// emulated seconds per ROM because oam_stress runs long.

namespace
{

constexpr std::uint8_t TestRunning = 0x80;
constexpr std::uint8_t ResetRequested = 0x81;
constexpr std::uint16_t StatusAddress = 0x6000;
constexpr std::uint16_t TextAddress = 0x6004;
constexpr std::uint16_t ResultAddress = 0x00F0;
constexpr std::uint16_t FrameworkResultAddress = 0x00F8;
constexpr std::size_t MaximumPeriod = 8;
constexpr std::uint32_t StablePeriodCycles = 60'000;
constexpr std::uint8_t PassingResult = 1;
constexpr std::uint64_t CpuClockHz = 1'789'773;
constexpr std::uint64_t ResetDelayCycles = CpuClockHz / 10;
constexpr std::uint64_t MaximumCycles = CpuClockHz * 60;

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

bool RunTestFile(const std::filesystem::path& path);

bool RunTest(const std::filesystem::path& path)
{
    try
    {
        return RunTestFile(path);
    }
    catch (const std::exception& error)
    {
        std::cerr << path.filename().string() << ": " << error.what() << std::endl;
        return false;
    }
}

bool RunTestFile(const std::filesystem::path& path)
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
    std::array<std::uint16_t, MaximumPeriod> recentProgramCounters{};
    std::array<std::uint32_t, MaximumPeriod + 1> periodStreaks{};

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

        if (started)
        {
            if (status < TestRunning)
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
            continue;
        }

        const std::uint16_t programCounter = console.Cpu().ProgramCounter();
        const std::size_t slot = cycle % MaximumPeriod;
        for (std::size_t period = 2; period <= MaximumPeriod; ++period)
        {
            const std::size_t previous = (cycle + MaximumPeriod - period) %
                                         MaximumPeriod;
            periodStreaks[period] =
                cycle >= period &&
                        programCounter == recentProgramCounters[previous]
                    ? periodStreaks[period] + 1
                    : 0;
        }
        recentProgramCounters[slot] = programCounter;

        bool stable = false;
        for (std::size_t period = 2; period <= MaximumPeriod; ++period)
        {
            stable = stable || periodStreaks[period] >= StablePeriodCycles;
        }
        if (!stable)
        {
            continue;
        }

        std::uint8_t result =
            console.ReadCpuRamForDiagnostics(FrameworkResultAddress);
        if (result == 0)
        {
            result = console.ReadCpuRamForDiagnostics(ResultAddress);
        }
        std::cout << path.filename().string() << ": "
                  << (result == PassingResult ? "passed" : "failed")
                  << " (code " << static_cast<int>(result) << ")\n";
        return result == PassingResult;
    }

    std::cerr << path.filename().string()
              << ": timed out before a final status (last PC $"
              << std::hex << recentProgramCounters[0] << std::dec
              << ", $F8="
              << static_cast<int>(console.ReadCpuRamForDiagnostics(
                     FrameworkResultAddress))
              << ", $F0="
              << static_cast<int>(console.ReadCpuRamForDiagnostics(
                     ResultAddress))
              << ")\n";
    return false;
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: DendyForgePpuRunner <test.nes> [...]\n";
        return 2;
    }

    bool passed = true;
    for (int index = 1; index < argc; ++index)
    {
        passed = RunTest(argv[index]) && passed;
    }
    return passed ? 0 : 1;
}
