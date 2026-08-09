#include <doctest/doctest.h>

#include "ines/ines_reader.hpp"
#include "cartridge/cartridge.hpp"
#include "cpu/cpu_test_support.hpp"

TEST_CASE("iNES reader loads the CPU test cartridge")
{
    dendyforge::INesReader reader;
    const auto path = dendyforge::test::RomPath("cpu_test.nes");

    REQUIRE_MESSAGE(reader.Load(path.string()), "Unable to load ROM: " << path.string());

    dendyforge::Cartridge cartridge(
        reader.Header(),
        reader.TakePRGRom(),
        reader.TakeCHRRom());

    CHECK(cartridge.Info().Mapper() == 0);
    CHECK(cartridge.PRGRom().size() == 32 * 1024);
    CHECK(cartridge.CHRRom().size() == 8 * 1024);
}
