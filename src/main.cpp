#include <iostream>

#include "ines/ines_reader.hpp"
#include "cartridge/cartridge_info.hpp"
#include "cartridge/cartridge.hpp"

int main()
{
    dendyforge::INesReader reader;

    if (!reader.Load("roms/BattleCity.nes"))
    {
        std::cout << "Failed to load ROM\n";
        return 1;
    }

    std::cout << "ROM loaded successfully\n";

    dendyforge::Cartridge cartridge(
       reader.Header(),
       reader.TakePRGRom(),
       reader.TakeCHRRom());

    const auto& info = cartridge.Info();

    std::cout << "\n=== iNES Header ===\n";

    std::cout << "PRG ROM    : "
              << static_cast<int>(cartridge.PRGRomBanks())
              << " x 16 KB\n";

    std::cout << "CHR ROM    : "
              << static_cast<int>(cartridge.CHRRomBanks())
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

    std::cout << "\nLoaded data:\n";

    std::cout << "PRG bytes : "
              << cartridge.PRGRom().size()
              << '\n';

    std::cout << "CHR bytes : "
              << cartridge.CHRRom().size()
              << '\n';

    return 0;
}
