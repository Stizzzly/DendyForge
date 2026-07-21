#include <iostream>

#include "ines/ines_reader.hpp"
#include "cartridge/cartridge_info.hpp"
#include "bus/bus.hpp"
#include "cartridge/cartridge.hpp"
#include "cpu/cpu6502.hpp"

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

    std::cout << "\nCartridge test:\n";

    std::uint8_t data;

    if (cartridge.CpuRead(0x8000, data))
    {
        std::cout << "CPU $8000 -> $"
                  << std::hex
                  << static_cast<int>(data)
                  << '\n';
    }
    else
    {
        std::cout << "CpuRead failed\n";
    }

    dendyforge::Bus bus;

    bus.CpuWrite(0x0000, 0xAA);

    std::cout << "\nRAM test:\n";

    std::cout << "$0000 -> $"
              << std::hex
              << static_cast<int>(bus.CpuRead(0x0000))
              << '\n';

    bus.CpuWrite(0x0000, 0x55);

    std::cout << "$0800 -> $"
              << std::hex
              << static_cast<int>(bus.CpuRead(0x0800))
              << '\n';

    std::cout << "$1000 -> $"
              << static_cast<int>(bus.CpuRead(0x1000))
              << '\n';

    std::cout << "$1800 -> $"
              << static_cast<int>(bus.CpuRead(0x1800))
              << '\n';

    bus.InsertCartridge(&cartridge);

    std::cout << "\nBus cartridge test:\n";

    std::cout << "CPU $8000 -> $"
              << std::hex
              << static_cast<int>(bus.CpuRead(0x8000))
              << '\n';
    std::cout << "\nCPU Status Register test:\n";

    dendyforge::CPU6502 cpu;

    cpu.SetFlag(dendyforge::CPU6502::Flags::C, true);
    cpu.SetFlag(dendyforge::CPU6502::Flags::Z, true);

    std::cout << "Carry : "
              << cpu.GetFlag(dendyforge::CPU6502::Flags::C)
              << '\n';

    std::cout << "Zero  : "
              << cpu.GetFlag(dendyforge::CPU6502::Flags::Z)
              << '\n';

    std::cout << "Negative : "
              << cpu.GetFlag(dendyforge::CPU6502::Flags::N)
              << '\n';

    cpu.SetFlag(dendyforge::CPU6502::Flags::C, false);

    std::cout << "\nAfter clearing Carry:\n";

    std::cout << "Carry : "
              << cpu.GetFlag(dendyforge::CPU6502::Flags::C)
              << '\n';

    std::cout << "Zero  : "
              << cpu.GetFlag(dendyforge::CPU6502::Flags::Z)
              << '\n';

    return 0;
}
