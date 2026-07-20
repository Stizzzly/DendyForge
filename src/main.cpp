#include <iostream>

#include "ines/ines_reader.hpp"
#include "cartridge/cartridge_info.hpp"
#include "cartridge/cartridge.hpp"
#include "mapper/mapper0.hpp"

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

    dendyforge::Mapper0 mapper(
        cartridge.PRGRomBanks(),
        cartridge.CHRRomBanks());

    std::uint32_t mapped = 0;

    mapper.CpuRead(0x8000, mapped);

    std::cout << "8000 -> "
              << std::hex
              << mapped
              << '\n';

    mapper.CpuRead(0x8001, mapped);

    std::cout << "8001 -> "
              << std::hex
              << mapped
              << '\n';

    mapper.CpuRead(0xC000, mapped);

    std::cout << "C000 -> "
              << std::hex
              << mapped
              << '\n';

    mapper.CpuRead(0xFFFF, mapped);

    std::cout << "FFFF -> "
              << std::hex
              << mapped
              << '\n';

    mapper.PpuRead(0x0000, mapped);

    std::cout << "PPU 0000 -> "
              << std::hex
              << mapped
              << '\n';

    mapper.PpuRead(0x1FFF, mapped);

    std::cout << "PPU 1FFF -> "
              << std::hex
              << mapped
              << '\n';

    return 0;
}
