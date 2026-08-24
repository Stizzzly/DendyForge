#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "console/console.hpp"
#include "cpu/cpu_test_support.hpp"

namespace dendyforge::test
{

struct BlarggResult
{
    bool detected{false};
    std::uint8_t status{0xFF};
    std::string output;
};

struct BlarggRunOptions
{
    std::size_t clockLimit{100'000'000};
    bool resetWhenRequested{false};
};

inline std::string ReadBlarggOutput(Console& console)
{
    std::string output;
    constexpr std::uint16_t kOutputStart = 0x6004;
    constexpr std::size_t kOutputLimit = 512;

    for (std::size_t offset = 0; offset < kOutputLimit; ++offset)
    {
        const auto value = console.ReadCartridgeRamForDiagnostics(
            static_cast<std::uint16_t>(kOutputStart + offset));
        if (value == 0)
        {
            break;
        }

        output.push_back(static_cast<char>(value));
    }

    return output;
}

inline BlarggResult RunBlarggRom(std::string_view relativeRomPath,
                                 BlarggRunOptions options = {})
{
    // The protocol is documented by Blargg: $6001-$6003 is DE B0 61,
    // $6000 is $80 while running and 0 on success; text begins at $6004.
    constexpr std::uint16_t kStatusAddress = 0x6000;
    constexpr std::size_t kPollPeriod = 256;
    // Blargg asks the user to delay reset for 100 ms. At the NTSC CPU clock
    // rate this is just under 179,000 CPU clocks, so leave a safe margin.
    constexpr std::size_t kResetDelayClocks = 200'000;

    Console console;
    if (!console.LoadRom(RomPath(relativeRomPath).string()))
    {
        return {};
    }

    bool resetRequested = false;
    bool resetIssued = false;
    std::size_t resetRequestClock = 0;

    for (std::size_t clock = 0; clock < options.clockLimit; ++clock)
    {
        console.Clock();

        if ((clock % kPollPeriod) != 0)
        {
            continue;
        }

        const bool isBlarggRom =
            console.ReadCartridgeRamForDiagnostics(0x6001) == 0xDE &&
            console.ReadCartridgeRamForDiagnostics(0x6002) == 0xB0 &&
            console.ReadCartridgeRamForDiagnostics(0x6003) == 0x61;
        if (!isBlarggRom)
        {
            continue;
        }

        const auto status = console.ReadCartridgeRamForDiagnostics(kStatusAddress);
        if (status == 0x81 && options.resetWhenRequested && !resetIssued)
        {
            if (!resetRequested)
            {
                resetRequested = true;
                resetRequestClock = clock;
            }
            else if (clock - resetRequestClock >= kResetDelayClocks)
            {
                console.Reset();
                resetIssued = true;
            }
            continue;
        }

        if (status <= 0x7F)
        {
            return {.detected = true, .status = status, .output = ReadBlarggOutput(console)};
        }
    }

    return {};
}

} // namespace dendyforge::test
