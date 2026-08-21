#include <doctest/doctest.h>

#include "console/console.hpp"
#include "cpu/cpu_test_support.hpp"

TEST_CASE("Console loads a Mapper 0 ROM and connects the CPU to its bus")
{
    dendyforge::Console console;

    REQUIRE(console.LoadRom(dendyforge::test::RomPath("cpu_test.nes").string()));

    CHECK(console.Cpu().Cycles() == 8);
    console.Clock();
    CHECK(console.Cpu().Cycles() == 7);
}
