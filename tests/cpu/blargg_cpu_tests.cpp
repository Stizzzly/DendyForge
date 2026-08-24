#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "console/console.hpp"
#include "cpu/cpu_test_support.hpp"

namespace
{

struct BlarggResult
{
    bool detected{false};
    std::uint8_t status{0xFF};
    std::string output;
};

std::string ReadBlarggOutput(dendyforge::Console& console)
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

BlarggResult RunBlarggRom(std::string_view romName)
{
    // The ROM protocol is documented by Blargg: $6001-$6003 is DE B0 61,
    // $6000 is $80 while running and 0 on success; text begins at $6004.
    constexpr std::uint16_t kStatusAddress = 0x6000;
    constexpr std::size_t kClockLimit = 100'000'000;
    constexpr std::size_t kPollPeriod = 256;

    dendyforge::Console console;
    if (!console.LoadRom(
        dendyforge::test::RomPath("blargg_instr_test_v5/" + std::string(romName)).string()))
    {
        return {};
    }

    for (std::size_t clock = 0; clock < kClockLimit; ++clock)
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
        if (status <= 0x7F)
        {
            return {.detected = true, .status = status, .output = ReadBlarggOutput(console)};
        }
    }

    return {};
}

void CheckBlarggPass(std::string_view romName)
{
    const auto result = RunBlarggRom(romName);
    REQUIRE_MESSAGE(result.detected, "Blargg result protocol was not reached before timeout");
    CHECK_MESSAGE(result.status == 0,
                  romName,
                  " failed with Blargg status ",
                  static_cast<unsigned>(result.status),
                  ": ",
                  result.output);
}

} // namespace

TEST_CASE("Console passes Blargg NES instruction tests v5")
{
    SUBCASE("official instructions")
    {
        CheckBlarggPass("official_only.nes");
    }

    SUBCASE("official and unofficial instructions")
    {
        CheckBlarggPass("all_instrs.nes");
    }
}
