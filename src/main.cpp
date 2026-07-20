#include <iostream>

#include "ines/ines_reader.hpp"
#include "cartridge/cartridge_info.hpp"

int main()
{
    dendyforge::INesReader reader;

    if (!reader.Load("roms/BattleCity.nes"))
    {
        std::cout << "Failed to load ROM\n";
        return 1;
    }

    std::cout << "ROM loaded successfully\n";

    dendyforge::CartridgeInfo info(reader.Header());

    std::cout << "\n=== iNES Header ===\n";

    std::cout << "PRG ROM    : "
              << static_cast<int>(reader.Header().prgRomBanks)
              << " x 16 KB\n";

    std::cout << "CHR ROM    : "
              << static_cast<int>(reader.Header().chrRomBanks)
              << " x 8 KB\n";

    std::cout << "Mapper     : "
              << static_cast<int>(info.Mapper())
              << '\n';

    std::cout << "Battery    : "
              << (info.HasBattery() ? "Yes" : "No")
              << '\n';

    std::cout << "Trainer    : "
              << (info.HasTrainer() ? "Yes" : "No")
              << '\n';

    return 0;
}
