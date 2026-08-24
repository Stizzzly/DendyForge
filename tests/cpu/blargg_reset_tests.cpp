#include <doctest/doctest.h>

#include <string>

#include "cpu/blargg_test_support.hpp"

TEST_CASE("Console preserves 2A03 state through reset")
{
    for (const auto romName : {"registers.nes", "ram_after_reset.nes"})
    {
        const auto result = dendyforge::test::RunBlarggRom(
            "blargg_reset/" + std::string(romName),
            {.resetWhenRequested = true});
        REQUIRE_MESSAGE(result.detected,
                        romName,
                        ": Blargg result protocol was not reached before timeout");
        CHECK_MESSAGE(result.status == 0,
                      romName,
                      " failed with Blargg status ",
                      static_cast<unsigned>(result.status),
                      ": ",
                      result.output);
    }
}
