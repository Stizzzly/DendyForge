#include "cartridge_tests.hpp"

#include <iostream>

#include "ines/ines_reader.hpp"
#include "cartridge/cartridge.hpp"

void RunCartridgeTests()
{
    std::cout << "\n=== Cartridge Tests ===\n";

    dendyforge::INesReader reader;

    if (!reader.Load("tests/cpu/roms/cpu_test.nes"))
    {
        std::cout << "FAIL: ROM loading\n";
        return;
    }

    dendyforge::Cartridge cartridge(
        reader.Header(),
        reader.TakePRGRom(),
        reader.TakeCHRRom());

    std::cout << "PASS: ROM loading\n";

    std::cout
        << "Mapper : "
        << static_cast<int>(cartridge.Info().Mapper())
        << '\n';

    std::cout
        << "PRG ROM : "
        << cartridge.PRGRom().size()
        << '\n';

    std::cout
        << "CHR ROM : "
        << cartridge.CHRRom().size()
        << '\n';
}
