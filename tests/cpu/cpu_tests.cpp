#include "cpu_tests.hpp"
#include <iostream>
#include <iomanip>

#include "bus/bus.hpp"
#include "cartridge/cartridge.hpp"
#include "ines/ines_reader.hpp"
#include "cpu/cpu6502.hpp"

namespace
{
void ExecuteInstruction(dendyforge::CPU6502& cpu)
{
    cpu.Clock();

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }
}
}
void ExecuteInstructions(dendyforge::CPU6502& cpu, int count)
{
    while (count--)
    {
        ExecuteInstruction(cpu);
    }
}
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

    //
    // Сбрасываем стартовые 8 циклов Reset
    //
    while (cpu.Cycles() > 0)
        cpu.Clock();

    //
    // Выполняем Fetch первой инструкции
    //
    cpu.Clock();

    std::cout
        << "PC = $"
        << std::uppercase
        << std::hex
        << std::setw(4)
        << std::setfill('0')
        << cpu.ProgramCounter()
        << '\n';

    std::cout
        << "Opcode = $"
        << std::setw(2)
        << static_cast<int>(cpu.Opcode())
        << '\n';
}

void TestDecode()
{
    std::cout << "\nDecode\n";

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

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    cpu.Clock();

    std::cout
        << "Opcode      = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(cpu.Opcode())
        << '\n';

    std::cout
        << "Instruction = "
        << cpu.CurrentInstruction()
        << '\n';

    std::cout
        << "Cycles left = "
        << std::dec
        << static_cast<int>(cpu.Cycles())
        << '\n';
}

void TestLDA()
{
    std::cout << "\nLDA\n";

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

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }
    ExecuteInstructions(cpu, 6);
    std::cout
        << "A = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(cpu.Accumulator())
        << '\n';

    std::cout
        << "Zero = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::Z)
        << '\n';

    std::cout
        << "Negative = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::N)
        << '\n';
}

void TestLDX()
{
    std::cout << "\nLDX\n";

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

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    ExecuteInstructions(cpu, 7);
    std::cout
        << "X = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(cpu.X())
        << '\n';

    std::cout
        << "Zero = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::Z)
        << '\n';

    std::cout
        << "Negative = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::N)
        << '\n';
}

void TestLDY()
{
    std::cout << "\nLDY\n";

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

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    ExecuteInstructions(cpu, 10);
    std::cout
        << "Y = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(cpu.Y())
        << '\n';

    std::cout
        << "Zero = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::Z)
        << '\n';

    std::cout
        << "Negative = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::N)
        << '\n';
}

void TestStore()
{
    std::cout << "\nSTA,STX,STY\n";

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

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    ExecuteInstructions(cpu, 11);
    std::uint8_t data = 0;

    data = bus.CpuRead(0x0200);

    std::cout
        << "$0200 = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(data)
        << '\n';

    data = bus.CpuRead(0x0201);

    std::cout
        << "$0201 = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(data)
        << '\n';

    data = bus.CpuRead(0x0202);

    std::cout
        << "$0202 = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(data)
        << '\n';

    std::cout << "\nSTA,STX,STY (ZP0)\n";

    ExecuteInstructions(cpu, 6);
    data = bus.CpuRead(0x0000);
    std::cout << "$0000 = $"
          << std::hex
          << std::uppercase
          << std::setw(2)
          << std::setfill('0')
          << static_cast<int>(data)
          << '\n';

    data = bus.CpuRead(0x0001);
    std::cout << "$0001 = $"
          << std::hex
          << std::uppercase
          << std::setw(2)
          << std::setfill('0')
          << static_cast<int>(data)
          << '\n';

    data = bus.CpuRead(0x0002);
    std::cout << "$0002 = $"
          << std::hex
          << std::uppercase
          << std::setw(2)
          << std::setfill('0')
          << static_cast<int>(data)
          << '\n';

    std::cout << "\nSTA,STX,STY (ZPX/ZPY)\n";

    ExecuteInstructions(cpu, 9);
    data = bus.CpuRead(0x0015);
    std::cout << "$0015 = $"
          << std::hex
          << std::uppercase
          << std::setw(2)
          << std::setfill('0')
          << static_cast<int>(data)
          << '\n';

    data = bus.CpuRead(0x0023);
    std::cout << "$0023 = $"
          << std::hex
          << std::uppercase
          << std::setw(2)
          << std::setfill('0')
          << static_cast<int>(data)
          << '\n';

    data = bus.CpuRead(0x0037);
    std::cout << "$0037 = $"
          << std::hex
          << std::uppercase
          << std::setw(2)
          << std::setfill('0')
          << static_cast<int>(data)
          << '\n';

    std::cout << "\nZero Page Wrap\n";

    ExecuteInstructions(cpu, 6);
    data = bus.CpuRead(0x0008);
    std::cout << "$0008 = $"
          << std::hex
          << std::uppercase
          << std::setw(2)
          << std::setfill('0')
          << static_cast<int>(data)
          << '\n';

    data = bus.CpuRead(0x0010);
    std::cout << "$0010 = $"
          << std::hex
          << std::uppercase
          << std::setw(2)
          << std::setfill('0')
          << static_cast<int>(data)
          << '\n';
}

void TestIncrementDecrement()
{
    std::cout << "\nINX,INY,DEX,DEY\n";

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

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    ExecuteInstructions(cpu, 40);
    std::cout
        << "X = $"
        << std::hex
        << std::uppercase
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(cpu.X())
        << '\n';

    std::cout
        << "Y = $"
        << std::hex
        << std::uppercase
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(cpu.Y())
        << '\n';
}
void RunCpuTests()
{
    std::cout << "\n=== CPU Tests ===\n";

    TestFlags();
    TestReset();
    TestFetch();
    TestDecode();
    TestLDA();
    TestLDX();
    TestLDY();
    TestStore();
    TestIncrementDecrement();
}

