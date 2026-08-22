#include <doctest/doctest.h>

#include "console/console.hpp"
#include "cpu/cpu_test_support.hpp"

TEST_CASE("Console loads a Mapper 0 ROM and connects the CPU to its bus")
{
    dendyforge::Console console;

    REQUIRE(console.LoadRom(dendyforge::test::RomPath("cpu_test.nes").string()));

    // Reset is a seven-cycle sequenced sequence (Phase 5); the vector
    // loads on its final drain cycle.
    CHECK(console.Cpu().Cycles() == 7);
    console.Clock();
    CHECK(console.Cpu().Cycles() == 6);
}

TEST_CASE("Console diagnostics exposes cartridge PRG RAM only")
{
    dendyforge::Console console;
    REQUIRE(console.LoadRom(dendyforge::test::RomPath("cpu_test.nes").string()));

    CHECK(console.ReadCartridgeRamForDiagnostics(0x8000) == 0);
}

TEST_CASE("Console diagnostics exposes mirrored CPU RAM only")
{
    dendyforge::Console console;

    CHECK(console.ReadCpuRamForDiagnostics(0x0000) == 0);
    CHECK(console.ReadCpuRamForDiagnostics(0x0800) == 0);
    CHECK(console.ReadCpuRamForDiagnostics(0x2000) == 0);
}
