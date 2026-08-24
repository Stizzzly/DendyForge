#include <doctest/doctest.h>

#include <string>
#include <string_view>

#include "cpu/blargg_test_support.hpp"

namespace
{

void CheckBlarggPass(std::string_view romName)
{
    const auto result = dendyforge::test::RunBlarggRom(
        "blargg_timing/" + std::string(romName));
    REQUIRE_MESSAGE(result.detected, "Blargg result protocol was not reached before timeout");
    CHECK_MESSAGE(result.status == 0,
                  romName,
                  " failed with Blargg status ",
                  static_cast<unsigned>(result.status),
                  ": ",
                  result.output);
}

} // namespace

TEST_CASE("Console passes Blargg timing and dummy-read tests")
{
    SUBCASE("all instruction timings")
    {
        CheckBlarggPass("instr_timing.nes");
    }

    SUBCASE("address wrapping and dummy reads")
    {
        CheckBlarggPass("instr_misc.nes");
    }
}
