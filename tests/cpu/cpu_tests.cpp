#include "cpu_tests.hpp"
#include <iostream>
#include <iomanip>

#include "bus/bus.hpp"
#include "cartridge/cartridge.hpp"
#include "ines/ines_reader.hpp"
#include "cpu/cpu6502.hpp"

namespace
{

void TestFlags()
{
    std::cout << "\nFlags\n";

    dendyforge::CPU6502 cpu;

    cpu.SetFlag(dendyforge::CPU6502::Flags::C, true);
    cpu.SetFlag(dendyforge::CPU6502::Flags::Z, true);

    std::cout
        << "Carry    : "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::C)
        << '\n';

    std::cout
        << "Zero     : "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::Z)
        << '\n';

    std::cout
        << "Negative : "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::N)
        << '\n';

    cpu.SetFlag(dendyforge::CPU6502::Flags::C, false);

    std::cout << "\nAfter clearing Carry\n";

    std::cout
        << "Carry    : "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::C)
        << '\n';

    std::cout
        << "Zero     : "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::Z)
        << '\n';
}

void TestReset()
{
    std::cout << "\nReset\n";

    dendyforge::INesReader reader;

    if (!reader.Load("tests/cpu/roms/cpu_test.nes"))
    {
        std::cout << "Failed to load cpu_test.nes\n";
        return;
    }

    dendyforge::Cartridge cartridge(
        reader.Header(),
        reader.TakePRGRom(),
        reader.TakeCHRRom());

    dendyforge::Bus bus;
    bus.InsertCartridge(&cartridge);

    dendyforge::CPU6502 cpu;
    cpu.ConnectBus(&bus);

    cpu.Reset();

    std::cout
        << "PC      = $"
        << std::uppercase
        << std::hex
        << std::setw(4)
        << std::setfill('0')
        << cpu.ProgramCounter()
        << '\n';

    std::cout
        << "SP      = $"
        << std::setw(2)
        << static_cast<int>(cpu.StackPointer())
        << '\n';

    std::cout
        << "Status  = $"
        << std::setw(2)
        << static_cast<int>(cpu.Status())
        << '\n';

    std::cout
        << "Cycles  = "
        << std::dec
        << static_cast<int>(cpu.Cycles())
        << '\n';
}

void TestFetch()
{
    std::cout << "\nFetch\n";
}

void TestDecode()
{
    std::cout << "\nDecode\n";
}

void TestLDA()
{
    std::cout << "\nLDA\n";
}

}

void RunCpuTests()
{
    std::cout << "\n=== CPU Tests ===\n";

    TestFlags();
    TestReset();
    TestFetch();
    TestDecode();
    TestLDA();
}
